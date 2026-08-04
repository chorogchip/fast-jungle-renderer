#pragma once

#include <vector>

#include "FastJungle/core/math/AABB.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render {

    // Renderer-owned spatial data derived from the preserved static scene.
    // None of these bounds are authoritative cooked data.
    struct SceneDerivedData final {
        std::vector<math::AABB> submesh_bounds;
        std::vector<math::AABB> mesh_bounds;
        std::vector<math::AABB> prototype_bounds;
        std::vector<math::AABB> point_batch_bounds;
        std::vector<math::AABB> matrix_batch_bounds;
        math::AABB world_bounds;

        [[nodiscard]]
        static SceneDerivedData build(const scene::StaticScene& scene);
    };

} // namespace fjr::render
