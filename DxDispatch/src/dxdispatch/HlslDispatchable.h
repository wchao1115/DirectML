#pragma once

#include "CommandLineArgs.h"

class HlslDispatchable : public Dispatchable
{
public:
    HlslDispatchable(std::shared_ptr<Device> device, const Model::HlslDispatchableDesc& desc, const CommandLineArgs& args, IDxDispatchLogger* logger);

    void Initialize() final;
    void Bind(const Bindings& bindings, uint32_t iteration) final;
    void Dispatch(const Model::DispatchCommand& args, uint32_t iteration, DeferredBindings& deferredBinings) final;

    enum class BufferViewType
    {
        Typed,      // (RW)Buffer
        Structured, // (RW|Append|Consume)StructuredBuffer
        Raw         // (RW)ByteAddresBuffer
    };

    struct BindPoint
    {
        BufferViewType viewType;
        D3D12_DESCRIPTOR_RANGE_TYPE descriptorType;
        uint32_t offsetInDescriptorsFromTableStart;
        uint32_t structureByteStride;
        // Texture support (samplers deferred). When 'isTexture' is true the buffer-specific members 
        // (viewType/structureByteStride) are ignored for descriptor creation.
        bool isTexture = false;
        D3D_SRV_DIMENSION srvDimension = D3D_SRV_DIMENSION_UNKNOWN; // Valid if isTexture & descriptorType==SRV
    };

private:
    void CompileWithDxc();
    void CreateRootSignatureAndBindingMap();

private:
    std::shared_ptr<Device> m_device;
    Model::HlslDispatchableDesc m_desc;
    bool m_forceDisablePrecompiledShadersOnXbox;
    bool m_rootSigDefinedOnXbox;
    bool m_noPdb;
    Microsoft::WRL::ComPtr<ID3D12ShaderReflection> m_shaderReflection;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
    // Separate heap for samplers (D3D12 requires distinct heap type)
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_samplerDescriptorHeap;
    std::unordered_map<std::string, BindPoint> m_bindPoints;
    bool m_printHlslDisassembly = false;
    Microsoft::WRL::ComPtr<IDxDispatchLogger> m_logger;
    // Root parameter indices (descriptor tables) for CSU (CBV/SRV/UAV) and SAMPLER heaps.
    int m_csuRootParameterIndex = -1; // CBV/SRV/UAV descriptor table root parameter index
    int m_samplerRootParameterIndex = -1;
};