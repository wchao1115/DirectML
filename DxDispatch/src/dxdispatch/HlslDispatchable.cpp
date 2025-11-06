#include "pch.h"
#include "Adapter.h"
#include "Device.h"
#include "Model.h"
#include "CommandLineArgs.h"
#include "Dispatchable.h"
#include "HlslDispatchable.h"

using Microsoft::WRL::ComPtr;

HlslDispatchable::HlslDispatchable(std::shared_ptr<Device> device, const Model::HlslDispatchableDesc& desc, const CommandLineArgs& args, IDxDispatchLogger* logger)
    :   m_device(device),
        m_desc(desc),
        m_forceDisablePrecompiledShadersOnXbox(args.ForceDisablePrecompiledShadersOnXbox()),
        m_noPdb(args.NoPdb()),
        m_hlslLangVer(args.HlslLangVersion()),
        m_printHlslDisassembly(args.PrintHlslDisassembly()),
        m_reportReflection(args.ReportReflection()),
        m_logger(logger)
{
    // Fold any command line -D defines directly into appropriate argument vectors.
    for (const auto &define : args.GetDxcDefines())
    {
        if (m_desc.pipelineKind == Model::HlslDispatchableDesc::PipelineKind::Graphics)
        {
            m_desc.vsCompilerArgs.push_back("-D");
            m_desc.vsCompilerArgs.push_back(define);
            m_desc.psCompilerArgs.push_back("-D");
            m_desc.psCompilerArgs.push_back(define);
        }
        else
        {
            m_desc.compilerArgs.push_back("-D");
            m_desc.compilerArgs.push_back(define);
        }
    }
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

void HlslDispatchable::CreateRootSignatureAndBindingMap(std::string id)
{
    // For compute pipelines we only have m_shaderReflection.
    // For graphics we have vertex reflection in m_shaderReflection and optional pixel reflection in m_psShaderReflection. Merge resources.
    std::vector<D3D12_SHADER_INPUT_BIND_DESC> mergedInputDescs;
    mergedInputDescs.reserve(32);
    auto addReflectionResources = [&](Microsoft::WRL::ComPtr<ID3D12ShaderReflection> refl)
    {
        D3D12_SHADER_DESC sd = {};
        THROW_IF_FAILED(refl->GetDesc(&sd));
        for (UINT i = 0; i < sd.BoundResources; ++i)
        {
            D3D12_SHADER_INPUT_BIND_DESC bindDesc = {};
            THROW_IF_FAILED(refl->GetResourceBindingDesc(i, &bindDesc));
            mergedInputDescs.push_back(bindDesc);
        }
    };

    if (m_desc.pipelineKind == Model::HlslDispatchableDesc::PipelineKind::Graphics && m_psShaderReflection)
    {
        // Add VS then PS resources; skip duplicates (same name+type+space+bind point) keeping first occurrence.
        std::vector<D3D12_SHADER_INPUT_BIND_DESC> temp;
        addReflectionResources(m_shaderReflection); // VS
        addReflectionResources(m_psShaderReflection); // PS optional

        // Deduplicate while preserving order.
        std::unordered_map<std::string, size_t> seen;
        std::vector<D3D12_SHADER_INPUT_BIND_DESC> unique;
        unique.reserve(mergedInputDescs.size());
        for (auto &d : mergedInputDescs)
        {
            auto it = seen.find(d.Name);
            if (it == seen.end())
            {
                seen[d.Name] = unique.size();
                unique.push_back(d);
            }
            else
            {
                // Validate compatibility; if mismatch throw.
                auto &existing = unique[it->second];
                bool compatible = existing.Type == d.Type && existing.BindPoint == d.BindPoint && existing.Space == d.Space && existing.BindCount == d.BindCount;
                if (!compatible)
                {
                    throw std::invalid_argument(fmt::format("Resource '{}': incompatible duplicate between VS and PS (type/slot mismatch).", d.Name));
                }
            }
        }
        mergedInputDescs.swap(unique);
    }
    else
    {
        addReflectionResources(m_shaderReflection);
    }

    if (m_reportReflection && m_shaderReflection)
    {
        auto ReportShaderDesc = [&](Microsoft::WRL::ComPtr<ID3D12ShaderReflection> refl)->void
        {
            D3D12_SHADER_DESC shaderDesc = {};
            THROW_IF_FAILED(refl->GetDesc(&shaderDesc));
            struct Field { const char* name; UINT value; } fields[] = {
                {"ConstantBuffers", shaderDesc.ConstantBuffers},
                {"BoundResources", shaderDesc.BoundResources},
                {"InputParameters", shaderDesc.InputParameters},
                {"OutputParameters", shaderDesc.OutputParameters},
                {"InstructionCount", shaderDesc.InstructionCount},
                {"TempRegisterCount", shaderDesc.TempRegisterCount},
                {"TempArrayCount", shaderDesc.TempArrayCount},
                {"DefCount", shaderDesc.DefCount},
                {"DclCount", shaderDesc.DclCount},
                {"TextureNormalInstructions", shaderDesc.TextureNormalInstructions},
                {"TextureLoadInstructions", shaderDesc.TextureLoadInstructions},
                {"TextureCompInstructions", shaderDesc.TextureCompInstructions},
                {"TextureBiasInstructions", shaderDesc.TextureBiasInstructions},
                {"TextureGradientInstructions", shaderDesc.TextureGradientInstructions},
                {"FloatInstructionCount", shaderDesc.FloatInstructionCount},
                {"IntInstructionCount", shaderDesc.IntInstructionCount},
                {"UintInstructionCount", shaderDesc.UintInstructionCount},
                {"StaticFlowControlCount", shaderDesc.StaticFlowControlCount},
                {"DynamicFlowControlCount", shaderDesc.DynamicFlowControlCount},
                {"MacroInstructionCount", shaderDesc.MacroInstructionCount},
                {"ArrayInstructionCount", shaderDesc.ArrayInstructionCount},
                {"CutInstructionCount", shaderDesc.CutInstructionCount},
                {"EmitInstructionCount", shaderDesc.EmitInstructionCount},
                {"GSMaxOutputVertexCount", shaderDesc.GSMaxOutputVertexCount},
                {"PatchConstantParameters", shaderDesc.PatchConstantParameters},
                {"cGSInstanceCount", shaderDesc.cGSInstanceCount},
                {"cControlPoints", shaderDesc.cControlPoints},
                {"cBarrierInstructions", shaderDesc.cBarrierInstructions},
                {"cInterlockedInstructions", shaderDesc.cInterlockedInstructions},
                {"cTextureStoreInstructions", shaderDesc.cTextureStoreInstructions},
            };
            std::string json;
            json.reserve(560);
            json.append(fmt::format("\"{}\":{{", id));
            json.append(fmt::format("\"ShaderName\":\"{}\",", m_desc.sourcePath.stem().string()));
            json.append("\"Reflection\":{");
            for (size_t i = 0; i < std::size(fields); ++i)
            {
                if (i) json.append(", ");
                json.append(fmt::format("\"{}\":{}", fields[i].name, fields[i].value));
            }
            json.append("}},");
            m_logger->LogInfo(json.c_str());
        };

        // Always report vertex (m_shaderReflection). Pixel reported if present.
        ReportShaderDesc(m_shaderReflection);
        if (m_desc.pipelineKind == Model::HlslDispatchableDesc::PipelineKind::Graphics && m_psShaderReflection)
            ReportShaderDesc(m_psShaderReflection);
    }

    auto [allDescriptorRanges, bindPoints] = ReflectBindingData(gsl::make_span(mergedInputDescs));
    m_bindPoints = bindPoints;

    if (m_rootSignature)
        return;

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

void HlslDispatchable::CompileWithDxc(std::string id)
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

    std::vector<std::wstring> compilerArgs;
    std::wstring hv = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(m_hlslLangVer);
    compilerArgs.push_back(std::wstring(L"-HV ") + hv);
    for (const auto &argUtf8 : m_desc.compilerArgs)
    {
        compilerArgs.push_back(std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(argUtf8));
    }

#ifdef _GAMING_XBOX
    if (m_forceDisablePrecompiledShadersOnXbox)
        DisablePrecompiledShaderOnXbox(compilerArgs);
#endif

    // Automatically request debug information (PDB generation) unless the user disabled it with --no_pdb.
    // Only add -Zi if neither -Zi nor /Zi was already specified explicitly.
    if (!m_noPdb)
    {
        bool hasZi = false;
        for (auto &arg : compilerArgs)
        {
            if (arg == L"-Zi" || arg == L"/Zi")
            {
                hasZi = true;
                break;
            }
        }
        if (!hasZi)
        {
            compilerArgs.push_back(L"-Zi");
        }
    }

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
            std::wstring fullPath = L"D:\\temp\\";
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
    if (!m_forceDisablePrecompiledShadersOnXbox)
        CreateRootSignatureFromPrecompiledShaderOnXbox(result);
#endif

    CreateRootSignatureAndBindingMap(id);

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

// Compile graphics pipeline: mandatory VS, optional PS. Vertex reflection always primary; pixel reflection optional.
void HlslDispatchable::CompileGraphicsWithDxc(std::string id)
{
    if (!m_device->GetDxcCompiler())
    {
        throw std::runtime_error("DXC is not available for this platform");
    }

    auto Utf8ToWide = [](const std::string& s)
    { 
        return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(s); 
    };

    auto InjectStageArgs = [&](std::vector<std::string>& stageArgs, const std::string& entry, const char* profileFlag)
    {
        stageArgs.push_back("-HV " + m_hlslLangVer);

        bool hasE = false, hasT = false;
        for (size_t i = 0; i < stageArgs.size(); ++i)
        {
            std::string low = stageArgs[i];
            std::transform(low.begin(), low.end(), low.begin(), ::tolower);
            if (low == "-e" || low == "/e") 
            { 
                hasE = true; 
            }
            else if (low == "-t" || low == "/t") 
            { 
                hasT = true; 
            }
        }

        if (!hasE)
        {
            stageArgs.push_back("-E");
            stageArgs.push_back(entry);
        }

        if (!hasT)
        {
            stageArgs.push_back("-T");
            stageArgs.push_back(profileFlag);
        }
    };

    InjectStageArgs(m_desc.vsCompilerArgs, m_desc.vsEntryPoint, "vs_6_6");
    if (!m_desc.pixelShaderPath.empty())
    {
        InjectStageArgs(m_desc.psCompilerArgs, m_desc.psEntryPoint, "ps_6_6");
    }

    // Local compile lambda adapted for per-stage args.
    auto CompileOnePerStage = [&](const std::filesystem::path& path,
                                  const std::vector<std::string>& stageArgs,
                                  ComPtr<IDxcBlob>& outBlob,
                                  Microsoft::WRL::ComPtr<ID3D12ShaderReflection>* outReflection,
                                  bool createRootSigXbox = false) -> void
    {
        ComPtr<IDxcBlobEncoding> source;
        THROW_IF_FAILED(m_device->GetDxcUtils()->LoadFile(path.c_str(), nullptr, &source));
        DxcBuffer srcBuf{ source->GetBufferPointer(), source->GetBufferSize(), DXC_CP_ACP };

        std::vector<std::wstring> wargs;
        wargs.reserve(stageArgs.size());
        for (auto& a : stageArgs) 
            wargs.push_back(Utf8ToWide(a));

#ifdef _GAMING_XBOX
        if (m_forceDisablePrecompiledShadersOnXbox)
            DisablePrecompiledShaderOnXbox(wargs);
#endif

        if (!m_noPdb)
        {
            bool hasZi = false;
            for (auto& a : wargs)
            {
                if (a == L"-Zi" || a == L"/Zi") 
                { 
                    hasZi = true; 
                    break; 
                }
            }
            if (!hasZi) 
                wargs.push_back(L"-Zi");
        }

        std::vector<LPCWSTR> lpargs(wargs.size());
        for (size_t i = 0; i < wargs.size(); ++i) 
            lpargs[i] = wargs[i].c_str();

        ComPtr<IDxcResult> result;
        THROW_IF_FAILED(m_device->GetDxcCompiler()->Compile(
            &srcBuf,
            lpargs.data(),
            (UINT)lpargs.size(),
            m_device->GetDxcIncludeHandler(),
            IID_PPV_ARGS(&result)));

        ComPtr<IDxcBlobUtf8> errors;
        THROW_IF_FAILED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr));
        if (errors && errors->GetStringLength()) 
        { 
            m_logger->LogError(errors->GetStringPointer()); 
        }

        HRESULT status = S_OK; 
        THROW_IF_FAILED(result->GetStatus(&status));
        if (FAILED(status)) 
        { 
            throw std::runtime_error("Graphics shader compilation failed"); 
        }

        THROW_IF_FAILED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&outBlob), nullptr));

        if (outReflection)
        {
            ComPtr<IDxcBlob> reflBlob; 
            THROW_IF_FAILED(result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflBlob), nullptr));
            DxcBuffer reflBuf{ reflBlob->GetBufferPointer(), reflBlob->GetBufferSize(), DXC_CP_ACP };
            THROW_IF_FAILED(m_device->GetDxcUtils()->CreateReflection(&reflBuf, IID_PPV_ARGS(outReflection->ReleaseAndGetAddressOf())));
        }

#ifdef _GAMING_XBOX
        if (createRootSigXbox && !m_forceDisablePrecompiledShadersOnXbox)
            CreateRootSignatureFromPrecompiledShaderOnXbox(result);
#endif        
    };

    // Compile pixel stage first if present (optional), then vertex (primary reflection stored in m_shaderReflection)
    if (!m_desc.pixelShaderPath.empty())
    {
        CompileOnePerStage(m_desc.pixelShaderPath, m_desc.psCompilerArgs, m_psBlob, &m_psShaderReflection); // optional pixel reflection
    }
    CompileOnePerStage(m_desc.vertexShaderPath, m_desc.vsCompilerArgs, m_vsBlob, &m_shaderReflection, true);

    CreateRootSignatureAndBindingMap(id);

    // Build graphics PSO (minimal defaults)
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = m_rootSignature.Get();
    pso.VS = { m_vsBlob->GetBufferPointer(), m_vsBlob->GetBufferSize() };
    if (m_psBlob)
    {
        pso.PS = { m_psBlob->GetBufferPointer(), m_psBlob->GetBufferSize() };
    }
    else
    {
        pso.PS = { nullptr, 0 }; // VS-only pipeline
    }
    pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

    if (m_desc.dsvFormat.has_value())
    {
        pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        pso.DSVFormat = m_desc.dsvFormat.value();
    }
    else
    {
        D3D12_DEPTH_STENCIL_DESC ds{}; 
        ds.DepthEnable = FALSE; 
        ds.StencilEnable = FALSE; 
        pso.DepthStencilState = ds; 
        pso.DSVFormat = DXGI_FORMAT_UNKNOWN;
    }

    switch (m_desc.primitiveTopology)
    {
        case D3D_PRIMITIVE_TOPOLOGY_POINTLIST: 
            pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; 
            break;
        case D3D_PRIMITIVE_TOPOLOGY_LINELIST: 
            pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; 
            break;
        case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST: 
        default: 
            pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; 
            break;
    }

    pso.NumRenderTargets = (UINT)std::min<size_t>(m_desc.rtvFormats.size(), 8);
    for (UINT i = 0; i < pso.NumRenderTargets; i++) 
    {
        pso.RTVFormats[i] = m_desc.rtvFormats[i];
    }

    pso.SampleMask = UINT_MAX;
    pso.SampleDesc.Count = 1;
    THROW_IF_FAILED(m_device->D3D()->CreateGraphicsPipelineState(&pso, IID_GRAPHICS_PPV_ARGS(m_pipelineState.ReleaseAndGetAddressOf())));

    // Descriptor heaps (same logic as compute path)
    uint32_t numCSU = 0;
    uint32_t numSamplers = 0;
    for (auto& kv : m_bindPoints)
    {
        switch (kv.second.descriptorType)
        {
            case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER: 
                numSamplers += kv.second.bindCount; 
                break;
            case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
            case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
            case D3D12_DESCRIPTOR_RANGE_TYPE_UAV: 
                numCSU += kv.second.bindCount; 
                break;
            default: break;
        }
    }

    if (numCSU)
    {
        D3D12_DESCRIPTOR_HEAP_DESC h{};
        h.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        h.NumDescriptors = numCSU;
        h.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        THROW_IF_FAILED(m_device->D3D()->CreateDescriptorHeap(&h, IID_GRAPHICS_PPV_ARGS(m_descriptorHeap.ReleaseAndGetAddressOf())));
    }

    if (numSamplers)
    {
        D3D12_DESCRIPTOR_HEAP_DESC h{};
        h.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        h.NumDescriptors = numSamplers;
        h.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        THROW_IF_FAILED(m_device->D3D()->CreateDescriptorHeap(&h, IID_GRAPHICS_PPV_ARGS(m_samplerDescriptorHeap.ReleaseAndGetAddressOf())));
    }
}

void HlslDispatchable::Initialize(std::string id)
{
    if (m_desc.compiler != Model::HlslDispatchableDesc::Compiler::DXC)
    {
        throw std::invalid_argument("Only DXC compiler is supported.");
    }
    if (m_desc.pipelineKind == Model::HlslDispatchableDesc::PipelineKind::Graphics)
    {
        CompileGraphicsWithDxc(id);
    }
    else
    {
        CompileWithDxc(id);
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

    ID3D12DescriptorHeap* descriptorHeaps[2]; UINT heapCount = 0;
    if (m_descriptorHeap) descriptorHeaps[heapCount++] = m_descriptorHeap.Get();
    if (m_samplerDescriptorHeap) descriptorHeaps[heapCount++] = m_samplerDescriptorHeap.Get();
    if (heapCount) m_device->GetCommandList()->SetDescriptorHeaps(heapCount, descriptorHeaps);
    if (m_desc.pipelineKind == Model::HlslDispatchableDesc::PipelineKind::Graphics)
    {
        m_device->GetCommandList()->SetGraphicsRootSignature(m_rootSignature.Get());
        m_device->GetCommandList()->SetPipelineState(m_pipelineState.Get());
        if (m_csuRootParameterIndex >= 0) m_device->GetCommandList()->SetGraphicsRootDescriptorTable(m_csuRootParameterIndex, m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
        if (m_samplerRootParameterIndex >= 0) m_device->GetCommandList()->SetGraphicsRootDescriptorTable(m_samplerRootParameterIndex, m_samplerDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    }
    else
    {
        m_device->GetCommandList()->SetComputeRootSignature(m_rootSignature.Get());
        m_device->GetCommandList()->SetPipelineState(m_pipelineState.Get());
        if (m_csuRootParameterIndex >= 0) m_device->GetCommandList()->SetComputeRootDescriptorTable(m_csuRootParameterIndex, m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
        if (m_samplerRootParameterIndex >= 0) m_device->GetCommandList()->SetComputeRootDescriptorTable(m_samplerRootParameterIndex, m_samplerDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    }
}

void HlslDispatchable::Dispatch(const Model::DispatchCommand& args, uint32_t iteration, DeferredBindings& deferredBinings)
{
    if (m_desc.pipelineKind == Model::HlslDispatchableDesc::PipelineKind::Graphics)
    {
        // Issue a simple non-indexed draw. Vertex buffers & render targets are expected to be set up externally for now.
        if (m_desc.vertexCount == 0)
        {
            throw std::runtime_error("Graphics dispatchable missing vertexCount.");
        }
        m_device->GetCommandList()->IASetPrimitiveTopology(m_desc.primitiveTopology);
        m_device->GetCommandList()->DrawInstanced(m_desc.vertexCount, 1, 0, 0);
    }
    else
    {
        m_device->RecordDispatch(args.dispatchableName.c_str(), args.threadGroupCount[0], args.threadGroupCount[1], args.threadGroupCount[2]);
    }
}

#ifdef _GAMING_XBOX
void HlslDispatchable::CreateRootSignatureFromPrecompiledShaderOnXbox(ComPtr<IDxcResult> result)
{
    assert(result && !m_forceDisablePrecompiledShadersOnXbox && !m_rootSignature);

    ComPtr<IDxcBlob> rootSignatureBlob;
    if (FAILED(result->GetOutput(
        DXC_OUT_ROOT_SIGNATURE,
        IID_PPV_ARGS(&rootSignatureBlob),
        nullptr)))
    {
        return; // No embedded root signature.
    }

    if (!rootSignatureBlob || rootSignatureBlob->GetBufferSize() == 0)
        return;

    THROW_IF_FAILED(m_device->D3D()->CreateRootSignature(
        0,
        rootSignatureBlob->GetBufferPointer(),
        rootSignatureBlob->GetBufferSize(),
        IID_GRAPHICS_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf())));
}

void HlslDispatchable::DisablePrecompiledShaderOnXbox(std::vector<std::wstring>& compilerArgs)
{
    for (size_t i = 0; i + 1 < compilerArgs.size(); )
    {
        if ((compilerArgs[i] == L"-D" || compilerArgs[i] == L"/D"))
        {
            bool removeDefine = false;
            const std::wstring& defineArg = compilerArgs[i + 1];
            if (defineArg == L"__XBOX_STRIP_DXIL") // exact match
            {
                removeDefine = true;
            }
            else if (defineArg.find(L"__XBOX_DX12_ROOT_SIGNATURE") != std::wstring::npos) // substring match
            {
                removeDefine = true;
            }
            if (removeDefine)
            {
                compilerArgs.erase(compilerArgs.begin() + i, compilerArgs.begin() + i + 2);                    
                continue; // re-check current index after erase
            }
        }
        ++i;
    }

    compilerArgs.push_back(L"-D");
    compilerArgs.push_back(L"__XBOX_DISABLE_PRECOMPILE");
}
#endif