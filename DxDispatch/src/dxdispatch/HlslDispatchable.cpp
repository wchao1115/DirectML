#include "pch.h"
#include "Adapter.h"
#include "Device.h"
#include "Model.h"
#include "CommandLineArgs.h"
#include "Dispatchable.h"
#include "HlslDispatchable.h"

using Microsoft::WRL::ComPtr;

HlslDispatchable::HlslDispatchable(std::shared_ptr<Device> device, const Model::HlslDispatchableDesc& desc, const CommandLineArgs& args, IDxDispatchLogger* logger)
    : m_device(device), m_desc(desc), m_forceDisablePrecompiledShadersOnXbox(args.ForceDisablePrecompiledShadersOnXbox()), m_noPdb(args.NoPdb()), m_rootSigDefinedOnXbox(args.RootSigDefinedOnXbox()),
      m_printHlslDisassembly(args.PrintHlslDisassembly()), m_logger(logger)
{
}

// Buffer classification helpers (textures handled separately).
HlslDispatchable::BufferViewType GetViewType(const D3D12_SHADER_INPUT_BIND_DESC& desc)
{
    switch (desc.Type)
    {
    case D3D_SIT_TEXTURE: // Could be Buffer (Dimension==BUFFER) or real texture (handled elsewhere)
    case D3D_SIT_UAV_RWTYPED:
    case D3D_SIT_TBUFFER:
        return HlslDispatchable::BufferViewType::Typed;
    case D3D_SIT_CBUFFER:
    case D3D_SIT_STRUCTURED:
    case D3D_SIT_UAV_RWSTRUCTURED:
    case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
        return HlslDispatchable::BufferViewType::Structured;
    case D3D_SIT_BYTEADDRESS:
    case D3D_SIT_UAV_RWBYTEADDRESS:
        return HlslDispatchable::BufferViewType::Raw;
    default:
        throw std::invalid_argument("Shader input type is not supported for buffer classification");
    }
}

D3D12_DESCRIPTOR_RANGE_TYPE GetDescriptorRangeType(const D3D12_SHADER_INPUT_BIND_DESC& desc)
{
    switch (desc.Type)
    {
    case D3D_SIT_CBUFFER:
        return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    case D3D_SIT_TEXTURE:
    case D3D_SIT_STRUCTURED:
    case D3D_SIT_BYTEADDRESS:
    case D3D_SIT_TBUFFER:
        return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    case D3D_SIT_UAV_RWTYPED:
    case D3D_SIT_UAV_RWSTRUCTURED:
    case D3D_SIT_UAV_RWBYTEADDRESS:
    case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
        return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    case D3D_SIT_SAMPLER:
        return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    default:
        throw std::invalid_argument("Shader input type is not supported for descriptor range classification");
    }
}

using BindingData = std::tuple<
    std::vector<D3D12_DESCRIPTOR_RANGE1>, 
    std::unordered_map<std::string, HlslDispatchable::BindPoint>>;

// Reflects descriptor ranges and binding points from the HLSL source.
BindingData ReflectBindingData(gsl::span<D3D12_SHADER_INPUT_BIND_DESC> shaderInputDescs)
{    
    std::vector<D3D12_DESCRIPTOR_RANGE1> descriptorRanges; // all ranges (we'll partition later)
    std::unordered_map<std::string, HlslDispatchable::BindPoint> bindPoints;

    D3D12_DESCRIPTOR_RANGE1 currentRange = {};
    currentRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    uint32_t currentOffsetCSU = 0;      // CBV/SRV/UAV heap-relative
    uint32_t currentOffsetSampler = 0;  // SAMPLER heap-relative

    for (size_t resourceIndex = 0; resourceIndex < shaderInputDescs.size(); resourceIndex++)
    {
        const auto& shaderInputDesc = shaderInputDescs[resourceIndex];
        bool isTexture = false;
        D3D_SRV_DIMENSION srvDim = D3D_SRV_DIMENSION_UNKNOWN;

        if (shaderInputDesc.Type == D3D_SIT_TEXTURE && shaderInputDesc.Dimension != D3D_SRV_DIMENSION_BUFFER)
        {
            isTexture = true;
            srvDim = shaderInputDesc.Dimension;
        }

        auto rangeType = GetDescriptorRangeType(shaderInputDesc);
        auto numDescriptors = shaderInputDesc.BindCount;

        HlslDispatchable::BufferViewType viewType = HlslDispatchable::BufferViewType::Typed; // default; unused for textures & samplers
        uint32_t stride = 0;
        if (!isTexture && rangeType != D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
        {
            viewType = GetViewType(shaderInputDesc);
            stride = (viewType == HlslDispatchable::BufferViewType::Structured ? shaderInputDesc.NumSamples : 0);
        }

        HlslDispatchable::BindPoint bp = {};
        bp.viewType = viewType;
        bp.descriptorType = rangeType;
    bp.offsetInDescriptorsFromTableStart = (rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER) ? currentOffsetSampler : currentOffsetCSU;
        bp.structureByteStride = stride;
        bp.isTexture = isTexture;
        bp.srvDimension = srvDim;
        bindPoints[shaderInputDesc.Name] = bp;

        if (rangeType == currentRange.RangeType && shaderInputDesc.Space == currentRange.RegisterSpace)
        {
            currentRange.NumDescriptors += numDescriptors;
        }
        else
        {
            if (currentRange.NumDescriptors > 0)
            {
                descriptorRanges.push_back(currentRange);
            }

            currentRange.RangeType = rangeType;
            currentRange.NumDescriptors = numDescriptors;
            currentRange.RegisterSpace = shaderInputDesc.Space;
        }

        if (rangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
        {
            currentOffsetSampler += numDescriptors;
        }
        else
        {
            currentOffsetCSU += numDescriptors;
        }
    }

    if (currentRange.NumDescriptors > 0)
    {
        descriptorRanges.push_back(currentRange);
    }

    return std::make_tuple(descriptorRanges, bindPoints);
}

void HlslDispatchable::CreateRootSignatureAndBindingMap()
{
    D3D12_SHADER_DESC shaderDesc = {};
    THROW_IF_FAILED(m_shaderReflection->GetDesc(&shaderDesc));
    
    std::vector<D3D12_SHADER_INPUT_BIND_DESC> shaderInputDescs(shaderDesc.BoundResources);
    for (uint32_t resourceIndex = 0; resourceIndex < shaderDesc.BoundResources; resourceIndex++)
    {
        THROW_IF_FAILED(m_shaderReflection->GetResourceBindingDesc(resourceIndex, &shaderInputDescs[resourceIndex]));
    }

    auto [allDescriptorRanges, bindPoints] = ReflectBindingData(shaderInputDescs);
    m_bindPoints = bindPoints;

#ifdef _GAMING_XBOX
    if (m_rootSignature)
        return;
#endif

    std::vector<D3D12_ROOT_PARAMETER1> rootParameters;
    std::vector<D3D12_DESCRIPTOR_RANGE1> csuRanges;
    std::vector<D3D12_DESCRIPTOR_RANGE1> samplerRanges;
    for (auto& r : allDescriptorRanges)
    {
        if (r.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER) samplerRanges.push_back(r);
    else csuRanges.push_back(r);
    }
    if (!csuRanges.empty())
    {
        m_csuRootParameterIndex = static_cast<int>(rootParameters.size());
        D3D12_ROOT_PARAMETER1 rootParameter = {};
        rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(csuRanges.size());
        rootParameter.DescriptorTable.pDescriptorRanges = csuRanges.data();
        rootParameters.push_back(rootParameter);
    }
    if (!samplerRanges.empty())
    {
        m_samplerRootParameterIndex = static_cast<int>(rootParameters.size());
        D3D12_ROOT_PARAMETER1 rootParameter = {};
        rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(samplerRanges.size());
        rootParameter.DescriptorTable.pDescriptorRanges = samplerRanges.data();
        rootParameters.push_back(rootParameter);
    }

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rootSigDesc.Desc_1_1.NumParameters = static_cast<UINT>(rootParameters.size());
    rootSigDesc.Desc_1_1.pParameters = rootParameters.data();

    ComPtr<ID3DBlob> rootSignatureBlob;
    ComPtr<ID3DBlob> rootSignatureErrors;
#ifdef _GAMING_XBOX
    HRESULT hr = D3D12SerializeVersionedRootSignature(&rootSigDesc, &rootSignatureBlob, &rootSignatureErrors);
#else
    HRESULT hr = m_device->D3DModule()->SerializeVersionedRootSignature(&rootSigDesc, &rootSignatureBlob, &rootSignatureErrors);
#endif
    if (FAILED(hr))
    {
        if (rootSignatureErrors)
        {
            m_logger->LogError(static_cast<LPCSTR>(rootSignatureErrors->GetBufferPointer()));
        }
        THROW_HR(hr);
    }

    THROW_IF_FAILED(m_device->D3D()->CreateRootSignature(
        0, 
        rootSignatureBlob->GetBufferPointer(), 
        rootSignatureBlob->GetBufferSize(), 
        IID_GRAPHICS_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf())));
}

void HlslDispatchable::CompileWithDxc()
{
    if (!m_device->GetDxcCompiler())
    {
        throw std::runtime_error("DXC is not available for this platform");
    }

    ComPtr<IDxcBlobEncoding> source;
    THROW_IF_FAILED(m_device->GetDxcUtils()->LoadFile(
        m_desc.sourcePath.c_str(),
        nullptr, 
        &source));

    DxcBuffer sourceBuffer;
    sourceBuffer.Ptr = source->GetBufferPointer();
    sourceBuffer.Size = source->GetBufferSize();
    sourceBuffer.Encoding = DXC_CP_ACP;

    std::vector<std::wstring> compilerArgs(m_desc.compilerArgs.size());
    for (size_t i = 0; i < m_desc.compilerArgs.size(); i++)
    {
        compilerArgs[i] = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(m_desc.compilerArgs[i]);
    }

#ifdef _GAMING_XBOX
    if (m_forceDisablePrecompiledShadersOnXbox)
    {
        compilerArgs.push_back(L"-D");
        compilerArgs.push_back(L"__XBOX_DISABLE_PRECOMPILE");
    }
#endif

    std::vector<LPCWSTR> lpcwstrArgs(compilerArgs.size());
    for (size_t i = 0; i < compilerArgs.size(); i++)
    {
        lpcwstrArgs[i] = compilerArgs[i].data();
    }

    ComPtr<IDxcResult> result;
    THROW_IF_FAILED(m_device->GetDxcCompiler()->Compile(
        &sourceBuffer, 
        lpcwstrArgs.data(), 
        static_cast<UINT32>(lpcwstrArgs.size()), 
        m_device->GetDxcIncludeHandler(), 
        IID_PPV_ARGS(&result)));

    ComPtr<IDxcBlobUtf8> errors;
    THROW_IF_FAILED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr));
    if (errors != nullptr && errors->GetStringLength() != 0)
    {
        std::string errorsString{ errors->GetStringPointer() };
        m_logger->LogError(fmt::format("DXC failed to compile with errors: {}", errorsString).c_str());
    }

    HRESULT compileStatus = S_OK;
    THROW_IF_FAILED(result->GetStatus(&compileStatus));
    if (FAILED(compileStatus))
    {
        throw std::invalid_argument("Failed to compile.");
    }

    ComPtr<IDxcBlob> shaderBlob;
    THROW_IF_FAILED(result->GetOutput(
        DXC_OUT_OBJECT, 
        IID_PPV_ARGS(&shaderBlob), 
        nullptr));

    ComPtr<IDxcBlob> reflectionBlob;
    THROW_IF_FAILED(result->GetOutput(
        DXC_OUT_REFLECTION, 
        IID_PPV_ARGS(&reflectionBlob), 
        nullptr));

    if (!m_noPdb)
    {
        ComPtr<IDxcBlob> pdbBlob;
        ComPtr<IDxcBlobUtf16> pdbName;
        if (SUCCEEDED(result->GetOutput(
            DXC_OUT_PDB,
            IID_PPV_ARGS(&pdbBlob),
            &pdbName)))
        {
            // TODO: store this in a temp directory?
            FILE* fp = nullptr;
#ifdef _GAMING_XBOX
            std::wstring fullPath = L"T:\\"; // T:\ is writable in Xbox.
#else
            std::wstring fullPath;
#endif
            fullPath += pdbName->GetStringPointer();
            _wfopen_s(&fp, fullPath.c_str(), L"wb");
            fwrite(pdbBlob->GetBufferPointer(), pdbBlob->GetBufferSize(), 1, fp);
            fclose(fp);
        }
    }

    DxcBuffer reflectionBuffer;
    reflectionBuffer.Ptr = reflectionBlob->GetBufferPointer();
    reflectionBuffer.Size = reflectionBlob->GetBufferSize();
    reflectionBuffer.Encoding = DXC_CP_ACP;

    THROW_IF_FAILED(m_device->GetDxcUtils()->CreateReflection(
        &reflectionBuffer, 
        IID_PPV_ARGS(m_shaderReflection.ReleaseAndGetAddressOf())));

    if (m_printHlslDisassembly)
    {
        DxcBuffer bytecodeBuffer;
        bytecodeBuffer.Ptr = shaderBlob->GetBufferPointer();
        bytecodeBuffer.Size = shaderBlob->GetBufferSize();
        bytecodeBuffer.Encoding = DXC_CP_ACP;

        ComPtr<IDxcResult> result;
        THROW_IF_FAILED(m_device->GetDxcCompiler()->Disassemble(
            &bytecodeBuffer, 
            IID_PPV_ARGS(&result)
        ));

        ComPtr<IDxcBlob> disassemblyText;
        THROW_IF_FAILED(result->GetOutput(
            DXC_OUT_DISASSEMBLY, 
            IID_PPV_ARGS(&disassemblyText), 
            nullptr
        ));

        m_logger->LogInfo("---------------------------------------------------------");
        m_logger->LogInfo(static_cast<LPCSTR>(disassemblyText->GetBufferPointer()));
        m_logger->LogInfo("---------------------------------------------------------");
    }

#ifdef _GAMING_XBOX
    if (m_rootSigDefinedOnXbox)
    {
        ComPtr<IDxcBlob> rootSignatureBlob;
        THROW_IF_FAILED(result->GetOutput(
            DXC_OUT_ROOT_SIGNATURE,
            IID_PPV_ARGS(&rootSignatureBlob),
            nullptr));

        THROW_IF_FAILED(m_device->D3D()->CreateRootSignature(
            0,
            rootSignatureBlob->GetBufferPointer(),
            rootSignatureBlob->GetBufferSize(),
            IID_GRAPHICS_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf())));
    }
#endif
    CreateRootSignatureAndBindingMap();

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.CS.pShaderBytecode = shaderBlob->GetBufferPointer();
    psoDesc.CS.BytecodeLength = shaderBlob->GetBufferSize();
    THROW_IF_FAILED(m_device->D3D()->CreateComputePipelineState(
        &psoDesc,
        IID_GRAPHICS_PPV_ARGS(m_pipelineState.ReleaseAndGetAddressOf())));

    ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    // Create descriptor heaps (CSU + optional sampler)
    uint32_t numCSU = 0; // CBV, SRV, UAV
    uint32_t numSamplers = 0;
    for (auto& kv : m_bindPoints)
    {
        switch (kv.second.descriptorType)
        {
        case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER: numSamplers++; break;
        case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
        case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
        case D3D12_DESCRIPTOR_RANGE_TYPE_UAV: numCSU++; break;
        default: break;
        }
    }
    if (numCSU > 0)
    {
        D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
        descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        descriptorHeapDesc.NumDescriptors = numCSU;
        descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        THROW_IF_FAILED(m_device->D3D()->CreateDescriptorHeap(
            &descriptorHeapDesc, 
            IID_GRAPHICS_PPV_ARGS(m_descriptorHeap.ReleaseAndGetAddressOf())));
    }
    if (numSamplers > 0)
    {
        D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
        samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        samplerHeapDesc.NumDescriptors = numSamplers;
        samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        THROW_IF_FAILED(m_device->D3D()->CreateDescriptorHeap(
            &samplerHeapDesc,
            IID_GRAPHICS_PPV_ARGS(m_samplerDescriptorHeap.ReleaseAndGetAddressOf())));
    }
}

void HlslDispatchable::Initialize()
{
    if (m_desc.compiler == Model::HlslDispatchableDesc::Compiler::DXC)
    {
        CompileWithDxc();
    }
    else
    {
        throw std::invalid_argument("FXC isn't supported yet");
    }
}

void HlslDispatchable::Bind(const Bindings& bindings, uint32_t iteration)
{
    uint32_t descriptorIncrementSizeCSU = m_device->D3D()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    uint32_t descriptorIncrementSizeSampler = m_device->D3D()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    for (auto& binding : bindings)
    {
        auto& targetName = binding.first;
        auto& sources = binding.second;
        assert(sources.size() == 1); // TODO: support multiple
        auto& source = sources[0];

        assert(source.resource != nullptr);
        assert(source.resourceDesc != nullptr);

        // Validate resource type compatibility lazily per case below (treat all as const; we never mutate descriptors here).
        const Model::BufferDesc* sourceBufferDescPtr = std::get_if<Model::BufferDesc>(&source.resourceDesc->value);
        const Model::TextureDesc* sourceTextureDescPtr = std::get_if<Model::TextureDesc>(&source.resourceDesc->value);
        const Model::SamplerDesc* sourceSamplerDescPtr = std::get_if<Model::SamplerDesc>(&source.resourceDesc->value);

        auto& bindPointIterator = m_bindPoints.find(targetName);
        if (bindPointIterator == m_bindPoints.end())
        {
            throw std::invalid_argument(fmt::format("Attempting to bind shader input '{}', which does not exist (or was optimized away) in the shader.", targetName));
        }
        auto& bindPoint = bindPointIterator->second;

        auto GetCpuHandle = [&](bool sampler)->CD3DX12_CPU_DESCRIPTOR_HANDLE
        {
            if (sampler)
            {
                return CD3DX12_CPU_DESCRIPTOR_HANDLE{
                    m_samplerDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                    static_cast<int>(bindPoint.offsetInDescriptorsFromTableStart),
                    descriptorIncrementSizeSampler};
            }
            else
            {
                return CD3DX12_CPU_DESCRIPTOR_HANDLE{
                    m_descriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                    static_cast<int>(bindPoint.offsetInDescriptorsFromTableStart),
                    descriptorIncrementSizeCSU};
            }
        };

        auto FillBufferOrUavViewDesc = [&](auto& viewDesc)
        {
            viewDesc.Buffer.StructureByteStride = bindPoint.structureByteStride;
            if (source.elementCount > std::numeric_limits<uint32_t>::max())
            {
                throw std::invalid_argument(fmt::format("ElementCount '{}' is too large.", source.elementCount));
            }
            viewDesc.Buffer.NumElements = static_cast<uint32_t>(source.elementCount);
            viewDesc.Buffer.FirstElement = source.elementOffset;

            if (bindPoint.viewType == BufferViewType::Typed)
            {
                if (source.format)
                {
                    viewDesc.Format = *source.format;
                }
                else
                {
                    // If the binding doesn't specify, assume the data type used to initialize the buffer.
                    assert(sourceBufferDescPtr);
                    viewDesc.Format = Device::GetDxgiFormatFromDmlTensorDataType(sourceBufferDescPtr->initialValuesDataType);
                }
            }
            else if (bindPoint.viewType == BufferViewType::Structured)
            {
                if (source.format && *source.format != DXGI_FORMAT_UNKNOWN)
                {
                    throw std::invalid_argument(fmt::format("'{}' is a structured buffer, so the format must be omitted or UNKNOWN.", targetName));
                }
                viewDesc.Format = DXGI_FORMAT_UNKNOWN;
            }
            else if (bindPoint.viewType == BufferViewType::Raw)
            {
                if (source.format && *source.format != DXGI_FORMAT_R32_TYPELESS)
                {
                    throw std::invalid_argument(fmt::format("'{}' is a raw buffer, so the format must be omitted or R32_TYPELESS.", targetName));
                }

                assert(sourceBufferDescPtr);
                if (sourceBufferDescPtr->sizeInBytes % D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT != 0)
                {
                    throw std::invalid_argument(fmt::format(
                        "Attempting to bind '{}' as a raw buffer, but its size ({} bytes) is not aligned to {} bytes", 
                        source.resourceDesc->name,
                        sourceBufferDescPtr->sizeInBytes,
                        D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT));
                }

                viewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
                if constexpr (std::is_same_v<decltype(viewDesc), D3D12_UNORDERED_ACCESS_VIEW_DESC&>)
                {
                    viewDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
                }
                if constexpr (std::is_same_v<decltype(viewDesc), D3D12_SHADER_RESOURCE_VIEW_DESC&>)
                {
                    viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
                }
            }
        };

        auto FillTextureViewDesc = [&](D3D12_SHADER_RESOURCE_VIEW_DESC& viewDesc, const Model::TextureDesc& texDesc, const HlslDispatchable::BindPoint& bp)
        {
            viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            viewDesc.Format = texDesc.format;
            switch (bp.srvDimension)
            {
            case D3D_SRV_DIMENSION_TEXTURE2D:
                // D3D12_SHADER_RESOURCE_VIEW_DESC::ViewDimension uses D3D12_SRV_DIMENSION; cast/explicit enum to avoid mismatch.
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MipLevels = 1;
                viewDesc.Texture2D.MostDetailedMip = 0;
                break;
            default:
                throw std::invalid_argument("Only TEXTURE2D SRVs supported in A-C phase");
            }
        };

        if (bindPoint.descriptorType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
        {
            if (!sourceSamplerDescPtr)
            {
                throw std::invalid_argument(fmt::format("Binding '{}' expected a sampler resource", targetName));
            }
            auto& samp = *sourceSamplerDescPtr;
            D3D12_SAMPLER_DESC sd = {};
            sd.Filter = samp.filter;
            sd.AddressU = samp.addressU;
            sd.AddressV = samp.addressV;
            sd.AddressW = samp.addressW;
            sd.MipLODBias = samp.mipLODBias;
            sd.MaxAnisotropy = samp.maxAnisotropy;
            sd.ComparisonFunc = samp.comparisonFunc;
            memcpy(sd.BorderColor, samp.borderColor, sizeof(float)*4);
            sd.MinLOD = samp.minLOD;
            sd.MaxLOD = samp.maxLOD;
            auto cpuHandle = GetCpuHandle(true);
            m_device->D3D()->CreateSampler(&sd, cpuHandle);
        }
        else if (bindPoint.isTexture)
        {
            if (bindPoint.descriptorType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
            {
                throw std::invalid_argument("RW textures (UAV) not yet supported in A-C phase");
            }

            // Texture SRV
            if (!std::holds_alternative<Model::TextureDesc>(source.resourceDesc->value))
            {
                throw std::invalid_argument(fmt::format("Binding '{}' expected a texture resource", targetName));
            }
            auto& texDesc = std::get<Model::TextureDesc>(source.resourceDesc->value);

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            FillTextureViewDesc(srvDesc, texDesc, bindPoint);
            auto cpuHandle = GetCpuHandle(false);
            m_device->D3D()->CreateShaderResourceView(source.resource, &srvDesc, cpuHandle);
        }
        else if (bindPoint.descriptorType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
        {
            if (!sourceBufferDescPtr)
            {
                throw std::invalid_argument(fmt::format("Binding '{}' expected a buffer resource (UAV)", targetName));
            }
            auto& sourceBufferDesc = *sourceBufferDescPtr;
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            FillBufferOrUavViewDesc(uavDesc);
            uavDesc.Buffer.CounterOffsetInBytes = source.counterOffsetBytes;
            auto cpuHandle = GetCpuHandle(false);
            m_device->D3D()->CreateUnorderedAccessView(source.resource, source.counterResource, &uavDesc, cpuHandle);
        }
        else if (bindPoint.descriptorType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
        {
            if (!sourceBufferDescPtr && !bindPoint.isTexture)
            {
                throw std::invalid_argument(fmt::format("Binding '{}' expected a buffer resource (SRV)", targetName));
            }
            auto& sourceBufferDesc = *sourceBufferDescPtr; // safe if not texture
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            FillBufferOrUavViewDesc(srvDesc);
            auto cpuHandle = GetCpuHandle(false);
            m_device->D3D()->CreateShaderResourceView(source.resource, &srvDesc, cpuHandle);
        }
        else if (bindPoint.descriptorType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
        {
            if (!sourceBufferDescPtr)
            {
                throw std::invalid_argument(fmt::format("Binding '{}' expected a buffer resource (CBV)", targetName));
            }
            auto& sourceBufferDesc = *sourceBufferDescPtr;
            if (sourceBufferDesc.sizeInBytes % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT != 0)
            {
                throw std::invalid_argument(fmt::format(
                    "Attempting to bind '{}' as a constant buffer, but its size ({} bytes) is not aligned to {} bytes", 
                    source.resourceDesc->name,
                    sourceBufferDesc.sizeInBytes,
                    D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
            }
            else if (sourceBufferDesc.sizeInBytes > std::numeric_limits<uint32_t>::max())
            {
                throw std::invalid_argument(fmt::format(
                    "Attempting to bind '{}' as a constant buffer, but its size ({} bytes) is too large.", 
                    source.resourceDesc->name,
                    sourceBufferDesc.sizeInBytes));
            }

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = source.resource->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = static_cast<uint32_t>(sourceBufferDesc.sizeInBytes);
            auto cpuHandle = GetCpuHandle(false);
            m_device->D3D()->CreateConstantBufferView(&cbvDesc, cpuHandle);
        }
        else
        {
            throw std::invalid_argument("Unexpected binding type");
        }
    }

    m_device->GetCommandList()->SetComputeRootSignature(m_rootSignature.Get());
    m_device->GetCommandList()->SetPipelineState(m_pipelineState.Get());
    ID3D12DescriptorHeap* descriptorHeaps[2];
    UINT heapCount = 0;
    if (m_descriptorHeap) descriptorHeaps[heapCount++] = m_descriptorHeap.Get();
    if (m_samplerDescriptorHeap) descriptorHeaps[heapCount++] = m_samplerDescriptorHeap.Get();
    if (heapCount) m_device->GetCommandList()->SetDescriptorHeaps(heapCount, descriptorHeaps);
    if (m_csuRootParameterIndex >= 0) m_device->GetCommandList()->SetComputeRootDescriptorTable(m_csuRootParameterIndex, m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    if (m_samplerRootParameterIndex >= 0) m_device->GetCommandList()->SetComputeRootDescriptorTable(m_samplerRootParameterIndex, m_samplerDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
}

void HlslDispatchable::Dispatch(const Model::DispatchCommand& args, uint32_t iteration, DeferredBindings& deferredBinings)
{
    m_device->RecordDispatch(args.dispatchableName.c_str(), args.threadGroupCount[0], args.threadGroupCount[1], args.threadGroupCount[2]);
    m_device->ExecuteCommandListAndWait();
}