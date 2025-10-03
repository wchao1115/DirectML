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
        // Treat both SRV (TEXTURE) and UAV typed (UAV_RWTYPED) non-buffer dimensions as textures.
        if ((shaderInputDesc.Type == D3D_SIT_TEXTURE || shaderInputDesc.Type == D3D_SIT_UAV_RWTYPED) &&
            shaderInputDesc.Dimension != D3D_SRV_DIMENSION_BUFFER)
        {
            isTexture = true;
            srvDim = shaderInputDesc.Dimension; // Re-uses SRV dimension enum for UAV typed textures too.
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
        bp.bindCount = static_cast<uint32_t>(numDescriptors);
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
        if (r.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
            samplerRanges.push_back(r);
        else
            csuRanges.push_back(r);
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
    if (m_forceDisablePrecompiledShadersOnXbox && !m_rootSigDefinedOnXbox)
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
        case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER: numSamplers += kv.second.bindCount; break;
        case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
        case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
        case D3D12_DESCRIPTOR_RANGE_TYPE_UAV: numCSU += kv.second.bindCount; break;
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

    // Heap descriptor counts for bounds checking
    uint32_t heapCSUCapacity = m_descriptorHeap ? m_descriptorHeap->GetDesc().NumDescriptors : 0;
    uint32_t heapSamplerCapacity = m_samplerDescriptorHeap ? m_samplerDescriptorHeap->GetDesc().NumDescriptors : 0;

    for (auto& binding : bindings)
    {
        auto& targetName = binding.first;
        const auto& sources = binding.second; // May be >1 for descriptor arrays
        if (sources.empty())
        {
            throw std::invalid_argument(fmt::format("Binding '{}' supplied with zero sources.", targetName));
        }
        const auto& source = sources[0];
        assert(source.resource != nullptr);
        assert(source.resourceDesc != nullptr);

        // Validate resource type compatibility lazily per case below (treat all as const; we never mutate descriptors here).
        const Model::BufferDesc* sourceBufferDescPtr = std::get_if<Model::BufferDesc>(&source.resourceDesc->value);
        const Model::TextureDesc* sourceTextureDescPtr = std::get_if<Model::TextureDesc>(&source.resourceDesc->value);
        const Model::SamplerDesc* sourceSamplerDescPtr = std::get_if<Model::SamplerDesc>(&source.resourceDesc->value);

        auto bindPointIterator = m_bindPoints.find(targetName);
        if (bindPointIterator == m_bindPoints.end())
        {
            throw std::invalid_argument(fmt::format("Attempting to bind shader input '{}', which does not exist (or was optimized away) in the shader.", targetName));
        }
        auto& bindPoint = bindPointIterator->second;

        uint32_t expected = std::max<uint32_t>(1, bindPoint.bindCount);
        bool replicate = false;
        if (expected > 1)
        {
            if (sources.size() != 1)
            {
                throw std::invalid_argument(fmt::format(
                    "Binding '{}' is a descriptor array of size {}. Enumerating multiple distinct resources is unsupported; provide a single resource with 'replicate': true.",
                    targetName, expected));
            }
            if (!source.replicate)
            {
                throw std::invalid_argument(fmt::format(
                    "Binding '{}' requires 'replicate': true to populate descriptor array of size {}.",
                    targetName, expected));
            }
            replicate = true;
        }
        else
        {
            if (sources.size() != 1)
            {
                throw std::invalid_argument(fmt::format(
                    "Binding '{}' expects exactly one resource (got {}).", targetName, sources.size()));
            }
        }

        auto validateIndex = [&](uint32_t descriptorIndex, bool sampler)
        {
            if (sampler)
            {
                if (descriptorIndex >= heapSamplerCapacity)
                {
                    throw std::runtime_error(fmt::format("Sampler descriptor index {} out of range (capacity {}).", descriptorIndex, heapSamplerCapacity));
                }
            }
            else
            {
                if (descriptorIndex >= heapCSUCapacity)
                {
                    throw std::runtime_error(fmt::format("Descriptor index {} out of range (capacity {}).", descriptorIndex, heapCSUCapacity));
                }
            }
        };

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

        auto FillBufferOrUavViewDesc = [&](auto& viewDesc, const Dispatchable::BindingSource& src, const Model::BufferDesc* bufDescPtr)
        {
            viewDesc.Buffer.StructureByteStride = bindPoint.structureByteStride;
            if (src.elementCount > std::numeric_limits<uint32_t>::max())
            {
                throw std::invalid_argument(fmt::format("ElementCount '{}' is too large.", src.elementCount));
            }
            viewDesc.Buffer.NumElements = static_cast<uint32_t>(src.elementCount);
            viewDesc.Buffer.FirstElement = src.elementOffset;

            if (bindPoint.viewType == BufferViewType::Typed)
            {
                if (src.format)
                {
                    viewDesc.Format = *src.format;
                }
                else
                {
                    assert(bufDescPtr);
                    viewDesc.Format = Device::GetDxgiFormatFromDmlTensorDataType(bufDescPtr->initialValuesDataType);
                }
            }
            else if (bindPoint.viewType == BufferViewType::Structured)
            {
                if (src.format && *src.format != DXGI_FORMAT_UNKNOWN)
                {
                    throw std::invalid_argument(fmt::format("'{}' is a structured buffer, so the format must be omitted or UNKNOWN.", targetName));
                }
                viewDesc.Format = DXGI_FORMAT_UNKNOWN;
            }
            else if (bindPoint.viewType == BufferViewType::Raw)
            {
                if (src.format && *src.format != DXGI_FORMAT_R32_TYPELESS)
                {
                    throw std::invalid_argument(fmt::format("'{}' is a raw buffer, so the format must be omitted or R32_TYPELESS.", targetName));
                }
                assert(bufDescPtr);
                if (bufDescPtr->sizeInBytes % D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT != 0)
                {
                    throw std::invalid_argument(fmt::format(
                        "Attempting to bind '{}' as a raw buffer, but its size ({} bytes) is not aligned to {} bytes", 
                        src.resourceDesc->name,
                        bufDescPtr->sizeInBytes,
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
                if (texDesc.arraySize != 1)
                {
                    throw std::invalid_argument("Shader expects Texture2D view but resource has arraySize > 1 (should bind as Texture2DArray).");
                }
                // D3D12_SHADER_RESOURCE_VIEW_DESC::ViewDimension uses D3D12_SRV_DIMENSION; cast/explicit enum to avoid mismatch.
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MipLevels = 1;
                viewDesc.Texture2D.MostDetailedMip = 0;
                break;
            case D3D_SRV_DIMENSION_TEXTURE3D:
                if (texDesc.depth <= 1)
                {
                    throw std::invalid_argument("Shader expects Texture3D view but resource depth <= 1.");
                }
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
                viewDesc.Texture3D.MipLevels = 1;
                viewDesc.Texture3D.MostDetailedMip = 0;
                viewDesc.Texture3D.ResourceMinLODClamp = 0.0f;
                break;
            case D3D_SRV_DIMENSION_TEXTURE2DARRAY:
                if (texDesc.arraySize < 1)
                {
                    throw std::invalid_argument("Shader expects Texture2DArray view but resource arraySize < 1.");
                }
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                viewDesc.Texture2DArray.MostDetailedMip = 0;
                viewDesc.Texture2DArray.MipLevels = 1;
                viewDesc.Texture2DArray.FirstArraySlice = 0;
                viewDesc.Texture2DArray.ArraySize = texDesc.arraySize;
                viewDesc.Texture2DArray.PlaneSlice = 0;
                viewDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
                break;
            case D3D_SRV_DIMENSION_TEXTURECUBEARRAY:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
                viewDesc.TextureCubeArray.MostDetailedMip = 0;
                viewDesc.TextureCubeArray.MipLevels = 1; // Only 1 mip currently supported
                viewDesc.TextureCubeArray.ResourceMinLODClamp = 0.0f;
                viewDesc.TextureCubeArray.First2DArrayFace = 0;
                {
                    uint32_t numCubes = 0;
                    if (texDesc.isCubeArray && texDesc.cubeCount > 0)
                    {
                        numCubes = texDesc.cubeCount;
                    }
                    else if (texDesc.arraySize % 6 == 0 && texDesc.arraySize >= 6)
                    {
                        numCubes = texDesc.arraySize / 6;
                    }
                    if (numCubes == 0)
                    {
                        throw std::invalid_argument("Binding expects a TextureCubeArray but resource description lacks valid cubeCount/arraySize.");
                    }
                    viewDesc.TextureCubeArray.NumCubes = numCubes;
                }
                break;
            case D3D_SRV_DIMENSION_TEXTURECUBE:
                viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                viewDesc.TextureCube.MostDetailedMip = 0;
                viewDesc.TextureCube.MipLevels = 1;
                viewDesc.TextureCube.ResourceMinLODClamp = 0.0f;
                break;
            default:
                throw std::invalid_argument("Unsupported texture SRV dimension (supported: TEXTURE2D, TEXTURE3D, TEXTURECUBE, TEXTURECUBEARRAY)");
            }
        };

        // Helper to fetch resource index i (replicate if needed)
        auto getSource = [&](uint32_t i) -> const Dispatchable::BindingSource& {
            return replicate ? sources[0] : sources[i];
        };

        // Per-type creation handling with array support
        if (bindPoint.descriptorType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
        {
            for (uint32_t i = 0; i < expected; ++i)
            {
                const auto& src = getSource(i);
                const Model::SamplerDesc* sampPtr = std::get_if<Model::SamplerDesc>(&src.resourceDesc->value);
                if (!sampPtr)
                {
                    throw std::invalid_argument(fmt::format("Binding '{}' expected a sampler resource at index {}", targetName, i));
                }
                auto& samp = *sampPtr;
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
                uint32_t descriptorIndex = bindPoint.offsetInDescriptorsFromTableStart + i;
                validateIndex(descriptorIndex, true);
                auto base = m_samplerDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
                auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(base, descriptorIndex, descriptorIncrementSizeSampler);
                m_device->D3D()->CreateSampler(&sd, cpuHandle);
            }
        }
        else if (bindPoint.isTexture)
        {
            for (uint32_t i = 0; i < expected; ++i)
            {
                const auto& src = getSource(i);
                if (!std::holds_alternative<Model::TextureDesc>(src.resourceDesc->value))
                {
                    throw std::invalid_argument(fmt::format("Binding '{}' expected a texture resource at index {}", targetName, i));
                }
                auto& texDesc = std::get<Model::TextureDesc>(src.resourceDesc->value);
                uint32_t descriptorIndex = bindPoint.offsetInDescriptorsFromTableStart + i;
                validateIndex(descriptorIndex, false);
                auto base = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
                auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(base, descriptorIndex, descriptorIncrementSizeCSU);
                if (bindPoint.descriptorType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
                {
                    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                    uavDesc.Format = texDesc.format;
                    switch (bindPoint.srvDimension)
                    {
                    case D3D_SRV_DIMENSION_TEXTURE2D:
                        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                        uavDesc.Texture2D.MipSlice = 0;
                        uavDesc.Texture2D.PlaneSlice = 0;
                        break;
                    case D3D_SRV_DIMENSION_TEXTURE3D:
                        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
                        uavDesc.Texture3D.MipSlice = 0;
                        uavDesc.Texture3D.FirstWSlice = 0;
                        uavDesc.Texture3D.WSize = texDesc.depth;
                        break;
                    case D3D_SRV_DIMENSION_TEXTURE2DARRAY:
                        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                        uavDesc.Texture2DArray.MipSlice = 0;
                        uavDesc.Texture2DArray.FirstArraySlice = 0;
                        uavDesc.Texture2DArray.ArraySize = texDesc.arraySize;
                        uavDesc.Texture2DArray.PlaneSlice = 0;
                        break;
                    default:
                        throw std::invalid_argument("Unsupported UAV texture dimension (supported: TEXTURE2D, TEXTURE3D, TEXTURE2DARRAY)");
                    }
                    m_device->D3D()->CreateUnorderedAccessView(src.resource, nullptr, &uavDesc, cpuHandle);
                }
                else // SRV
                {
                    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                    FillTextureViewDesc(srvDesc, texDesc, bindPoint);
                    m_device->D3D()->CreateShaderResourceView(src.resource, &srvDesc, cpuHandle);
                }
            }
        }
        else if (bindPoint.descriptorType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
        {
            for (uint32_t i = 0; i < expected; ++i)
            {
                const auto& src = getSource(i);
                const Model::BufferDesc* bufDesc = std::get_if<Model::BufferDesc>(&src.resourceDesc->value);
                if (!bufDesc)
                {
                    throw std::invalid_argument(fmt::format("Binding '{}' expected a buffer resource (UAV) at index {}", targetName, i));
                }
                D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                FillBufferOrUavViewDesc(uavDesc, src, bufDesc);
                uavDesc.Buffer.CounterOffsetInBytes = src.counterOffsetBytes;
                uint32_t descriptorIndex = bindPoint.offsetInDescriptorsFromTableStart + i;
                validateIndex(descriptorIndex, false);
                auto base = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
                auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(base, descriptorIndex, descriptorIncrementSizeCSU);
                m_device->D3D()->CreateUnorderedAccessView(src.resource, src.counterResource, &uavDesc, cpuHandle);
            }
        }
        else if (bindPoint.descriptorType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
        {
            for (uint32_t i = 0; i < expected; ++i)
            {
                const auto& src = getSource(i);
                if (!bindPoint.isTexture)
                {
                    const Model::BufferDesc* bufDesc = std::get_if<Model::BufferDesc>(&src.resourceDesc->value);
                    if (!bufDesc)
                    {
                        throw std::invalid_argument(fmt::format("Binding '{}' expected a buffer resource (SRV) at index {}", targetName, i));
                    }
                    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                    FillBufferOrUavViewDesc(srvDesc, src, bufDesc);
                    uint32_t descriptorIndex = bindPoint.offsetInDescriptorsFromTableStart + i;
                    validateIndex(descriptorIndex, false);
                    auto base = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
                    auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(base, descriptorIndex, descriptorIncrementSizeCSU);
                    m_device->D3D()->CreateShaderResourceView(src.resource, &srvDesc, cpuHandle);
                }
                // Texture SRVs handled in bindPoint.isTexture path above
            }
        }
        else if (bindPoint.descriptorType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
        {
            for (uint32_t i = 0; i < expected; ++i)
            {
                const auto& src = getSource(i);
                const Model::BufferDesc* bufDesc = std::get_if<Model::BufferDesc>(&src.resourceDesc->value);
                if (!bufDesc)
                {
                    throw std::invalid_argument(fmt::format("Binding '{}' expected a buffer resource (CBV) at index {}", targetName, i));
                }
                if (bufDesc->sizeInBytes % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT != 0)
                {
                    m_logger->LogInfo(fmt::format(
                        "[warn] '{}' CBV size {} not {}-byte aligned; proceeding (perf mode).", 
                        src.resourceDesc->name,
                        bufDesc->sizeInBytes,
                        D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT).c_str());
                }
                else if (bufDesc->sizeInBytes > std::numeric_limits<uint32_t>::max())
                {
                    throw std::invalid_argument(fmt::format(
                        "Attempting to bind '{}' as a constant buffer, but its size ({} bytes) is too large.", 
                        src.resourceDesc->name,
                        bufDesc->sizeInBytes));
                }
                D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
                cbvDesc.BufferLocation = src.resource->GetGPUVirtualAddress();
                cbvDesc.SizeInBytes = static_cast<uint32_t>(bufDesc->sizeInBytes);
                uint32_t descriptorIndex = bindPoint.offsetInDescriptorsFromTableStart + i;
                validateIndex(descriptorIndex, false);
                auto base = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
                auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(base, descriptorIndex, descriptorIncrementSizeCSU);
                m_device->D3D()->CreateConstantBufferView(&cbvDesc, cpuHandle);
            }
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
}