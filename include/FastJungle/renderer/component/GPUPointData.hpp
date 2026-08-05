#pragma once

#include <DirectXMath.h>
#include <d3d12.h>

#include <cstdint>
#include <type_traits>
#include <array>

namespace fjr::render {

    inline constexpr std::uint32_t POINT_CLUSTER_SIZE = 256;
    inline constexpr std::uint32_t POINT_LOD_COUNT = 4;
    inline constexpr std::uint32_t POINT_PIPELINE_CLASS_COUNT = 2;
    inline constexpr std::uint32_t POINT_INVALID_BIN = UINT32_MAX;

    // PointBatch -> (PointCluster, new) -> PointInstance
    struct PointClusterGpu {
        DirectX::XMFLOAT3 bounds_center{};
        std::uint32_t point_batch_index = 0;

        DirectX::XMFLOAT3 bounds_extent{};
        std::uint32_t instance_offset = 0;
        std::uint32_t instance_count = 0;
        std::uint32_t padding[3]{};
    };

    // StaticScene PointBatch -> this new PointBatchGpu
    struct PointBatchGpu {
        DirectX::XMFLOAT4X4 part_local_transform{};
        DirectX::XMFLOAT4X4 batch_local_to_world{};

        // x: definition id
        // y: first bin id = pointBatchIndex * POINT_LOD_COUNT
        // z: first cluster id
        // w: cluster count
        DirectX::XMUINT4 indices{};

        // x: batch_local_to_world¿¡¼­ max scale
        DirectX::XMFLOAT4 culling{};
    };

    // definition AABB
    struct PointDefinitionGpu {
        // xyz: center
        // w: definition.local_transform amx scale
        DirectX::XMFLOAT4 bounds_center_scale{};
        DirectX::XMFLOAT4 bounds_extent{};

        // LODº° error
        DirectX::XMFLOAT4 lod_errors{};
    };

    struct PointDrawTemplateGpu {
        std::uint32_t bin_index = POINT_INVALID_BIN;
        std::uint32_t point_batch_index = 0;
        std::uint32_t material_id = 0;
        std::uint32_t pipeline_class = 0;

        std::uint32_t index_count = 0;
        std::uint32_t first_index = 0;
        std::int32_t base_vertex = 0;
        std::uint32_t padding = 0;
    };

    // root constant (3)
    struct PointIndirectCommand {
        std::uint32_t visible_instance_offset = 0;
        std::uint32_t material_id = 0;
        std::uint32_t point_batch_index = 0;
        D3D12_DRAW_INDEXED_ARGUMENTS draw{};
    };

    struct ScenePointResources {
        std::vector<PointClusterGpu> clusters;
        std::vector<PointBatchGpu> batches;
        std::vector<PointDefinitionGpu> definitions;
        std::vector<PointDrawTemplateGpu> draw_templates;

        std::array<uint32_t, POINT_PIPELINE_CLASS_COUNT> command_class_bases{};
        std::array<uint32_t, POINT_PIPELINE_CLASS_COUNT> command_class_capacities{};
        uint32_t bin_count = 0;
    };

    static_assert(sizeof(PointClusterGpu) == 48);
    static_assert(sizeof(PointBatchGpu) == 160);
    static_assert(sizeof(PointDefinitionGpu) == 48);
    static_assert(sizeof(PointDrawTemplateGpu) == 32);
    static_assert(sizeof(PointIndirectCommand) == 32);

    static_assert(std::is_trivially_copyable_v<PointClusterGpu>);
    static_assert(std::is_trivially_copyable_v<PointBatchGpu>);
    static_assert(std::is_trivially_copyable_v<PointDefinitionGpu>);
    static_assert(std::is_trivially_copyable_v<PointDrawTemplateGpu>);
    static_assert(std::is_trivially_copyable_v<PointIndirectCommand>);

} // namespace fjr::render