#pragma once

#include <filesystem>
#include <unordered_map>
#include <optional>
#include <variant>
#include <string>
#include <vector>
#include <gsl/gsl>
#include <DirectML.h>
#include "BucketAllocator.h"

class Model
{
public:
    // When binding a buffer to an operator it is possible to use a subregion of
    // the buffer by specifying an elementOffset, elementCount, and elementSizeInBytes.
    // Additionally, an optional format specifier dictates how to interpret the buffer
    // contents; when omitted the buffer will be interpreted using the same data type used
    // to initialize it.
    struct BufferBindingSource
    {
        std::string name;
        uint64_t elementCount;
        uint64_t elementSizeInBytes;
        uint64_t elementOffset;
        std::optional<DXGI_FORMAT> format;

        // For Append/Consume buffers only:
        std::optional<std::string> counterName;
        uint64_t counterOffsetBytes;

        std::vector<int64_t> shape;
        bool replicate = false; // If true and shader expects descriptor array, replicate this single source across all descriptors
    };

    using Bindings = std::unordered_map<std::string, std::vector<BufferBindingSource>>;

    // RESOURCES
    // ------------------------------------------------------------------------

    struct BufferDesc
    {
        uint64_t sizeInBytes;
        std::vector<std::byte> initialValues;
        DML_TENSOR_DATA_TYPE initialValuesDataType;
        uint64_t initialValuesOffsetInBytes;
        bool useDeferredBinding;
        // Structured constant buffer (field-accurate) metadata. When non-empty, the
        // 'initialValues' vector already contains every field's initialized bytes
        // placed at its reflected offset (including any padding ranges left as 0).
        struct ConstantBufferField
        {
            std::string name;   // Fully qualified field path (may be PADDING_* surrogate)
            std::string type;   // Coarse type string e.g. FLOAT, FLOAT3, UINT4
            uint32_t offset;    // Byte offset from start of cbuffer
            uint32_t size;      // Exact byte span occupied by this logical member (including its own padding)
        };
        std::vector<ConstantBufferField> cbufferFields; // Present only for structured cbuffers
        std::string structType; // Optional discriminator for ConstantBuffer<T> variants
        bool IsStructuredConstantBuffer() const { return !cbufferFields.empty(); }
    };

    // Texture resource description (moved out of ResourceDesc)
    struct TextureDesc
    {
        uint32_t width;
        uint32_t height;
        uint32_t depth = 1;              // For 3D textures (volume). 1 for 2D/cube.
        DXGI_FORMAT format;                 // e.g. DXGI_FORMAT_R8G8B8A8_UNORM
        std::vector<std::byte> initialData; // Optional initial texel data (row-major, tightly packed)
        bool useDeferredBinding = false;    // For parity with buffers (not yet implemented for textures)
        // Extended texture metadata (defaults preserve existing 2D single-mip behavior)
        uint32_t mipLevels = 1;             // Currently only 1 is supported for non-2D as well
        uint32_t arraySize = 1;             // For future 2D array support (still validated elsewhere)
        bool isCube = false;                // True if dim==Cube (arraySize implicitly 6)
        bool isCubeArray = false;           // True if dim==CubeArray (depthOrArraySize = cubeCount*6)
        uint32_t cubeCount = 0;             // Number of cubes when isCubeArray (must be >0); when isCube==true, cubeCount==1
        bool allowUav = false;              // True if texture should allow unordered access (RWTexture*)
    };
    
    // Sampler description (moved out of ResourceDesc)
    struct SamplerDesc
    {
        D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        D3D12_TEXTURE_ADDRESS_MODE addressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        D3D12_TEXTURE_ADDRESS_MODE addressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        D3D12_TEXTURE_ADDRESS_MODE addressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        float mipLODBias = 0.f;
        uint32_t maxAnisotropy = 1;
        D3D12_COMPARISON_FUNC comparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        float borderColor[4] = {0,0,0,0};
        float minLOD = 0.f;
        float maxLOD = D3D12_FLOAT32_MAX;
    };

    struct ResourceDesc
    {
        std::string name;
        std::variant<BufferDesc, TextureDesc, SamplerDesc> value;
    };

    // DISPATCHABLES
    // ------------------------------------------------------------------------

    struct HlslDispatchableDesc
    {
        enum class Compiler
        {
            DXC
        };

        enum class PipelineKind
        {
            Compute,
            Graphics
        };

        // Compiler model:
        //  - Compute: uses dispatchable-level compilerArgs (shared vector below)
        //  - Graphics: uses per-stage compiler argument vectors (vsCompilerArgs / psCompilerArgs)
        std::filesystem::path sourcePath; // Compute shader source when pipelineKind==Compute
        Compiler compiler;
        std::vector<std::string> compilerArgs; // Compute-only compiler arguments

        // Graphics extensions (optional). Presence of 'graphics' object with a vertex shader switches pipelineKind to Graphics. Pixel shader is optional.
        PipelineKind pipelineKind = PipelineKind::Compute;
        std::filesystem::path vertexShaderPath;    // Only valid when pipelineKind==Graphics
        std::filesystem::path pixelShaderPath;     // Only valid when pipelineKind==Graphics
        std::string vsEntryPoint = "main";
        std::string psEntryPoint = "main";
        std::vector<std::string> vsCompilerArgs;   // Graphics-only vertex stage args
        std::vector<std::string> psCompilerArgs;   // Graphics-only pixel stage args
        std::vector<DXGI_FORMAT> rtvFormats;       // Empty => compute; Graphics requires >=1
        std::optional<DXGI_FORMAT> dsvFormat;      // Optional depth/stencil format
        D3D_PRIMITIVE_TOPOLOGY primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        uint32_t vertexCount = 0;                  // For DrawInstanced (indexed draw unsupported in initial implementation)
    };

    struct DispatchableDesc
    {
        std::string name;
        std::variant<HlslDispatchableDesc> value;
    };

    // COMMANDS
    // ------------------------------------------------------------------------

    struct DispatchCommand
    {
        std::string dispatchableName;
        Bindings bindings;
        std::array<uint32_t, 3> threadGroupCount;
    };

    struct PrintCommand
    {
        std::string resourceName;
        bool verbose;
    };

    struct WriteFileCommand
    {
        std::string resourceName;
        std::string targetPath;
        std::vector<uint32_t> dimensions; // The resources don't store their dimensions. So repeat them here.
    };

    // Triggers submission of the current command list and resolution of any queued GPU timestamp queries.
    struct ResolveGpuTimeCommand
    {
        // No parameters.
    };

    using Command = std::variant<DispatchCommand, PrintCommand, WriteFileCommand, ResolveGpuTimeCommand>;

    struct CommandDesc
    {
        std::string type;
        std::string parameters;
        Command command;
    };

    Model() = default;

    Model(
        std::vector<ResourceDesc>&& resourceDescs,
        std::vector<DispatchableDesc>&& dispatchableDescs,
        std::vector<CommandDesc>&& commands,
        BucketAllocator&& allocator);

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) = default;
    Model& operator=(Model&&) = default;

    gsl::span<const ResourceDesc> GetResourceDescs() const { return m_resourceDescs; }
    gsl::span<const DispatchableDesc> GetDispatchableDescs() const { return m_dispatchableDescs; }
    gsl::span<const CommandDesc> GetCommands() const { return m_commands; }

    const ResourceDesc& GetResource(std::string_view name) const { return *m_resourceDescsByName.find(name.data())->second; }
    const DispatchableDesc& GetDispatchable(std::string_view name) const { return *m_dispatchableDescsByName.find(name.data())->second; }

private:
    std::vector<ResourceDesc> m_resourceDescs;
    std::vector<DispatchableDesc> m_dispatchableDescs;
    std::vector<CommandDesc> m_commands;
    BucketAllocator m_allocator;
    std::unordered_map<std::string, ResourceDesc*> m_resourceDescsByName;
    std::unordered_map<std::string, DispatchableDesc*> m_dispatchableDescsByName;
};