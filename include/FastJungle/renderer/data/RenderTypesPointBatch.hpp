#pragma once


#include <cstdint>
#include <type_traits>
#include <d3d12.h>
#include <DirectXMath.h>

#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render::data {

    // PointBatch -> (PointCluster, new) -> PointInstance
    struct StbufPointCluster {
        DirectX::XMFLOAT3 bounds_center{};
        uint32_t point_batch_index = 0;

        DirectX::XMFLOAT3 bounds_extent{};
        uint32_t instance_offset = 0;
        uint32_t instance_count = 0;
        uint32_t padding[3]{};
    };

    // StaticScene PointBatch -> this new PointBatchGpu
    struct StbufPointBatch {
        DirectX::XMFLOAT4X4 part_local_transform{};
        DirectX::XMFLOAT4X4 batch_local_to_world{};

        uint32_t definition_index = 0;
        uint32_t first_bin = 0;
        uint32_t first_cluster = 0;
        uint32_t cluster_count = 0;

        float transform_max_scale = 1.0f;
        float padding[3]{};
    };

    // definition AABB
    struct StbufPointDef {
        DirectX::XMFLOAT4 bounds_center_scale{};
        DirectX::XMFLOAT4 bounds_extent{};
        DirectX::XMFLOAT4 lod_errors{};
    };

    struct StbufPointDraw {
        uint32_t bin_index = Consts::IND_ERR;
        uint32_t point_batch_index = 0;
        uint32_t material_id = 0;
        EnumPointPSOClsas pipeline_class = EnumPointPSOClsas::SINGLE_SIDED;

        uint32_t index_count = 0;
        uint32_t first_index = 0;
        int32_t base_vertex = 0;
        uint32_t padding = 0;
    };

    // indirect root constant (3)
    struct IndirectbufPointBatch {
        std::uint32_t visible_instance_offset = 0;
        std::uint32_t material_id = 0;
        std::uint32_t point_batch_index = 0;
        D3D12_DRAW_INDEXED_ARGUMENTS draw{};

        static inline constexpr uint32_t CONSTANT_COUNT = 3;
    };

    static_assert(sizeof(StbufPointCluster) == 48);
    static_assert(sizeof(StbufPointBatch) == 160);
    static_assert(sizeof(StbufPointDef) == 48);
    static_assert(sizeof(IndirectbufPointBatch) == 32);

    static_assert(std::is_trivially_copyable_v<StbufPointCluster>);
    static_assert(std::is_trivially_copyable_v<StbufPointBatch>);
    static_assert(std::is_trivially_copyable_v<StbufPointDef>);
    static_assert(std::is_trivially_copyable_v<IndirectbufPointBatch>);

}