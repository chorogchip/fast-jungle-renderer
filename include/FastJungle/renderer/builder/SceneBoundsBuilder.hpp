#pragma once

#include <vector>

#include "FastJungle/core/math/AABB.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render {

    // StaticScene에 없음, cpu측 cluster 핸들
    struct PointClusterCpu {
        math::AABB world_bounds;
        std::uint32_t point_batch_index = 0;
        std::uint32_t instance_offset = 0;
        std::uint32_t instance_count = 0;
    };

    // Renderer-owned spatial data derived from the preserved static scene.
    // None of these bounds are authoritative cooked data.
    struct SceneBoundsBuilder final {
        std::vector<math::AABB> submesh_bounds;
        std::vector<math::AABB> mesh_bounds;

        std::vector<math::AABB> instanced_definition_bounds;
        std::vector<float> instanced_definition_max_scale;

        std::vector<math::AABB> point_batch_bounds;
        std::vector<float> point_batch_max_scale;  // cluster도입해서 더이상 메인으로 안쓰기

        // GPU 인스턴스별 컬링용 새거
        std::vector<float> point_batch_transform_max_scale;
        std::vector<PointClusterCpu> point_clusters;
        std::vector<scene::StaticScene::IndexRange> point_batch_cluster_ranges;

        std::vector<math::AABB> static_instance_bounds;
        std::vector<float> static_instance_max_scale;
        math::AABB world_bounds;

        [[nodiscard]]
        static SceneBoundsBuilder build(const scene::StaticScene& scene);
    };

} // namespace fjr::render
