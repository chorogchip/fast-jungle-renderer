#pragma once

#include <cstdint>
#include <DirectXMath.h>

namespace fjr::render::data {

    enum class EnumPointOrMatrix : uint32_t {
        POINT,
        MATRIX,
        COUNT,
    };

    enum class EnumPSOClass : uint32_t {
        SINGLE_SIDED,
        DOUBLE_SIDED,
        COUNT,
    };

    struct Consts {
        static inline constexpr uint32_t CBUF_ALIGN = 256; // TODO compile error
            // D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
        static inline constexpr uint32_t IND_ERR = UINT32_MAX;
        static inline constexpr uint64_t IND_ERR_64 = UINT64_MAX;
        static inline constexpr DirectX::XMFLOAT4X4 I_MAT{
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };

        static inline constexpr uint32_t PNT_CLUSTER_SZ = 256;
        static inline constexpr uint32_t LOD_CNT = 4;
        static inline constexpr uint32_t INSTANCE_CLASS_CNT =
            static_cast<uint32_t>(EnumPointOrMatrix::COUNT);
        static inline constexpr uint32_t PIPELINE_CNT =
            static_cast<uint32_t>(EnumPSOClass::COUNT);
    };

}