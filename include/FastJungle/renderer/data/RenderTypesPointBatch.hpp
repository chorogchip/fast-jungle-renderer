#pragma once

#include <array>
#include <cstdint>
#include <type_traits>
#include <d3d12.h>
#include <DirectXMath.h>

#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render::data {

    // PointBatch -> PointCluster -> PointInstance
    struct StbufPointCluster {
        DirectX::XMFLOAT3 bounds_center{};
        uint32_t point_mesh_batch_index = 0;

        DirectX::XMFLOAT3 bounds_extent{};
        uint32_t instance_offset = 0;
        uint32_t instance_count = 0;
        float world_max_scale = 0.0f;
        uint32_t padding[2]{};
    };

    struct StbufPointMeshBatch {
        DirectX::XMFLOAT4X4 local_transform{};

        uint32_t mesh_index = 0;
        uint32_t first_bin = 0;
        uint32_t first_cluster = 0;
        uint32_t cluster_count = 0;
    };

    // PointBatch-local culling definition.
    struct StbufPointDef {
        DirectX::XMFLOAT4 bounds_center_lod_scale{};
        DirectX::XMFLOAT4 bounds_extent_radius{};
        DirectX::XMFLOAT4 lod_errors{};
    };

    struct StbufPointDraw {
        uint32_t bin_index = Consts::IND_ERR;
        uint32_t draw_id = Consts::IND_ERR;
        EnumRasterClass raster_class =
            EnumRasterClass::OPAQUE_SINGLE_SIDED;
        uint32_t padding = 0;
    };

    struct IndirectDrawCommand {
        std::uint32_t draw_id = Consts::IND_ERR;
        D3D12_DRAW_INDEXED_ARGUMENTS draw{};

        static inline constexpr uint32_t CONSTANT_COUNT = 1;
    };

    struct IndirectCommandLayout {

        struct IndirectCommandRange {
            uint32_t first_command = 0;
            uint32_t max_command_count = 0;
        };

        std::array<IndirectCommandRange, Consts::RASTER_CLASS_CNT>
            class_ranges{};
        uint32_t total_command_capacity = 0;
    };

    static_assert(sizeof(StbufPointCluster) == 48);
    static_assert(sizeof(StbufPointMeshBatch) == 80);
    static_assert(sizeof(StbufPointDef) == 48);
    static_assert(sizeof(StbufPointDraw) == 16);
    static_assert(sizeof(IndirectDrawCommand) == 24);

    static_assert(std::is_trivially_copyable_v<StbufPointCluster>);
    static_assert(std::is_trivially_copyable_v<StbufPointMeshBatch>);
    static_assert(std::is_trivially_copyable_v<StbufPointDef>);
    static_assert(std::is_trivially_copyable_v<IndirectDrawCommand>);

}
