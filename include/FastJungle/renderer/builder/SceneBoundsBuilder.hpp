#pragma once

#include <vector>

#include "FastJungle/core/math/AABB.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render {

    // Renderer-owned spatial data derived from the preserved static scene.
    // None of these bounds are authoritative cooked data.
    struct SceneBoundsBuilder final {
        std::vector<math::AABB> submesh_bounds;
        std::vector<math::AABB> mesh_bounds;
        std::vector<math::AABB> instanced_definition_bounds;
        std::vector<math::AABB> point_batch_bounds;
        std::vector<float> point_batch_max_scale;
        std::vector<math::AABB> static_instance_bounds;
        std::vector<float> static_instance_max_scale;
        math::AABB world_bounds;

        [[nodiscard]]
        static SceneBoundsBuilder build(const scene::StaticScene& scene);
    };

} // namespace fjr::render
