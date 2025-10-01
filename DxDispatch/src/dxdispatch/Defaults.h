// Centralized default / magic values for DxDispatch HLSL-only path.
// Rationale: keep performance test harness predictable & easy to tweak.
// NOTE: Adjust here instead of scattering literals across the code base.

#pragma once

#include <cstdint>
#include <DirectML.h>

namespace dxdispatch::defaults
{
    // Default element size (bytes) for inferred buffers when not specified.
    inline constexpr uint32_t kInferredElementSizeBytes = 4; // float32

    // Default tensor data type used for inferred buffers with no explicit initializer.
    inline constexpr DML_TENSOR_DATA_TYPE kInferredDataType = DML_TENSOR_DATA_TYPE_FLOAT32;

    // Required constant buffer alignment (D3D12 spec) – we round inferred buffers up to this.
    inline constexpr uint32_t kConstantBufferAlignment = 256;

    // Whether to emit an info log when a resource is inferred.
    inline constexpr bool kLogInferredResources = true;

    // Helper: align 'value' up to alignment (power of two).
    inline constexpr uint64_t AlignUp(uint64_t value, uint64_t alignment)
    {
        return (value + (alignment - 1)) & ~(alignment - 1);
    }
}
