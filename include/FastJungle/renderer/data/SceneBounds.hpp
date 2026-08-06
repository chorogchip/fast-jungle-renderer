#pragma once

#include <cstdint>
#include <vector>

#include "FastJungle/core/math/AABB.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render::data {

    struct SceneBounds {

        struct PointClusterBounds {
            math::AABB world_bounds;
            std::uint32_t point_mesh_batch_index = 0;
            scene::StaticScene::EnumPointCategory category =
                scene::StaticScene::EnumPointCategory::COUNT;
            scene::StaticScene::IndexRange instances;
            float world_max_scale = 0.0f;
        };

        struct GeometryBounds {
            std::vector<math::AABB> submesh_bounds;
            std::vector<math::AABB> mesh_bounds;

        };

        struct PointBounds {
            std::vector<math::AABB> local_bounds;
            std::vector<float> local_max_scale;
            std::vector<float> local_sphere_radius;
            std::vector<math::AABB> batch_bounds;
            std::vector<float> batch_max_scale;

            std::vector<PointClusterBounds> clusters;
            std::vector<scene::StaticScene::IndexRange>
                batch_cluster_ranges;
        };

        struct StaticInstanceBounds {
            std::vector<math::AABB> bounds;
            std::vector<float> max_scale;
        };

        GeometryBounds geometry;
        PointBounds points;
        StaticInstanceBounds static_instances;
        math::AABB world_bounds;
    };

}
