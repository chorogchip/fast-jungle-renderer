#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/data/FrameConstData.hpp"

namespace fjr::render::data {


    struct PointCullingBatch {
        // Range in PointCullingBuildContext::instance_order.
        scene::StaticScene::IndexRange instances;
    };

    struct PointCullingInstance {
        uint32_t point_mesh_batch_index = Consts::IND_ERR;
        scene::StaticScene::EnumPointCategory category = scene::StaticScene::EnumPointCategory::COUNT;
    };

    struct PointCullingData {
        std::vector<PointCullingBatch> batches;
        std::vector<uint32_t> instance_order;
        std::vector<PointCullingInstance> instances;
    };

} // namespace fjr::render::data
