#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include "FastJungle/renderer/SceneResources.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render {

    struct SceneDrawItem {
        SceneResources::InstanceKind instance_kind =
            SceneResources::InstanceKind::POINT;

        uint32_t index_count = 0;
        uint32_t first_index = 0;
        int32_t base_vertex = 0;

        uint32_t lod_index = 0;
        // pointÀÏ‹š »ç¿ë, id in (batch, lod) permutation
        uint32_t instance_bin_index = scene::StaticScene::INVALID_INDEX;

        uint32_t instance_count = 0;
        SceneResources::DrawConstants constants{};

        // Index of a 256-byte-aligned record in either the point or matrix
        // transform constant buffer.
        uint32_t transform_constant_index = 0;

        // Index into point_batch_bounds or static_instance_bounds, selected
        // by instance_kind.
        uint32_t bounds_index = scene::StaticScene::INVALID_INDEX;

        float lod_error = 0.0f;
        float next_lod_error = std::numeric_limits<float>::infinity();

        scene::StaticScene::EnumSubmeshFlag flags =
            scene::StaticScene::EnumSubmeshFlag::DEFAULT;
    };

    struct SceneRenderData {
        std::vector<SceneDrawItem> draw_items;
        std::vector<SceneResources::PointDrawConstants> point_draw_constants;
        std::vector<SceneResources::MatrixDrawConstants> matrix_draw_constants;
        std::vector<SceneResources::MatrixInstance> matrix_instances;
        std::vector<SceneResources::Material> materials;
        std::vector<SceneResources::TextureBinding> texture_bindings;
    };

} // namespace fjr::render
