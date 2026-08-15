#pragma once

#include <cstdint>
#include <DirectXMath.h>

namespace fjr::render::data {

    enum class EnumInstanceKind : uint32_t {
        POINT,
        MATRIX,
        COUNT,
    };

    enum class EnumRasterClass : uint32_t {
        PYRAMID,
        TERRAIN,
        OPAQUE_SINGLE_SIDED,
        RIVER,
        ALPHA_TESTED,
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
        static inline constexpr uint32_t CULL_MAX_CONVENTIONAL_LODS = 7;
        static inline constexpr uint32_t CULL_MAX_IMPOSTOR_DIRECTIONS = 8;
        static inline constexpr uint32_t CULL_BUCKET_COUNT =
            CULL_MAX_CONVENTIONAL_LODS + CULL_MAX_IMPOSTOR_DIRECTIONS;
        static inline constexpr uint32_t CULL_RESERVATION_STRIDE = 16;
        static inline constexpr uint32_t INSTANCE_KIND_CNT =
            static_cast<uint32_t>(EnumInstanceKind::COUNT);
        static inline constexpr uint32_t RASTER_CLASS_CNT =
            static_cast<uint32_t>(EnumRasterClass::COUNT);

        static inline constexpr uint32_t SW_CLUSTER_VERTEX_COUNT = 64;
        static inline constexpr uint32_t SW_CLUSTER_TRIANGLE_COUNT = 128;
        static inline constexpr uint32_t SW_TRIANGLE_BITS = 7;
        static inline constexpr uint32_t SW_LOCAL_WORK_BITS = 17;
        static inline constexpr uint32_t SW_BATCH_BITS = 8;
        static inline constexpr uint32_t SW_LOCAL_WORK_CAPACITY =
            1u << SW_LOCAL_WORK_BITS;
        static inline constexpr uint32_t SW_BATCH_CAPACITY =
            1u << SW_BATCH_BITS;
    };

    static_assert(
        Consts::SW_TRIANGLE_BITS +
        Consts::SW_LOCAL_WORK_BITS +
        Consts::SW_BATCH_BITS == 32);

    static_assert(Consts::PNT_CLUSTER_SZ <= 256);
    static_assert(Consts::CULL_BUCKET_COUNT <=
        Consts::CULL_RESERVATION_STRIDE);

}
