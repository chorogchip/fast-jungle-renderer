#pragma once

#include <array>
#include <vector>

#include "FastJungle/core/math/AABB.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"
#include "FastJungle/renderer/data/RenderTypesCommon.hpp"
#include "FastJungle/renderer/data/RenderTypesPointBatch.hpp"

namespace fjr::render::data {

    struct SceneResourcesTemp {

        struct IndirectCommandRange {
            uint32_t first_command = 0;
            uint32_t max_command_count = 0;
        };

        struct PointIndirectLayout {
            std::array<IndirectCommandRange, Consts::PNT_PIPELINE_CNT>
                class_ranges{};
            uint32_t total_command_capacity = 0;
        };

        struct PointRenderPlan {
            std::vector<StbufPointBatch> clusters;
            std::vector<IndirectbufPointBatch> batches;
            std::vector<StbufPointDef> definitions;
            std::vector<StbufPointDraw> draw_templates;

            uint32_t bin_count = 0;
            PointIndirectLayout indirect_layout;
        };

        std::vector<StbufMaterial> materials;
        std::vector<StbufTextureBinding> texture_bindings;
        std::vector<StbufMatrixInstance> matrix_instances;
        std::vector<CbufPointDraw> point_constants;
        std::vector<CbufMatrixDraw> matrix_constants;

        PointRenderPlan points;
        math::AABB world_bounds;
    };
}