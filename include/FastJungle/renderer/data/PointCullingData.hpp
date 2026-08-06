#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/data/FrameConstData.hpp"

namespace fjr::render::data {


    struct PointCullingBatch {
        uint32_t point_batch_index = Consts::IND_ERR;
        scene::StaticScene::IndexRange instance_order_id;
    };

    struct PointCullingData {
        std::vector<PointCullingBatch> batches;
        std::vector<uint32_t> instance_order;
    };

} // namespace fjr::render::data
