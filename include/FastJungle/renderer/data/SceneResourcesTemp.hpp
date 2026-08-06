#pragma once

#include <vector>

#include "FastJungle/renderer/data/RenderConsts.hpp"
#include "FastJungle/renderer/data/RenderTypesCommon.hpp"
#include "FastJungle/renderer/data/RenderTypesPointBatch.hpp"

namespace fjr::render::data {

    struct SceneResourcesTemp {

        struct PointRenderPlan {
            std::vector<StbufPointCluster> clusters;
            std::vector<StbufPointMeshBatch> mesh_batches;
            std::vector<StbufPointDef> definitions;
            std::vector<StbufPointDraw> draw_templates;

            uint32_t bin_count = 0;
            IndirectCommandLayout indirect_layout;
        };

        std::vector<StbufMaterial> materials;
        std::vector<StbufTextureBinding> texture_bindings;
        std::vector<StbufMatrixInstance> matrix_instances;
        std::vector<CbufPointDraw> point_constants;
        std::vector<CbufMatrixDraw> matrix_constants;

        PointRenderPlan points;
    };
}
