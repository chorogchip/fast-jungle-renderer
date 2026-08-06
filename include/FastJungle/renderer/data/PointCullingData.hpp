#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render::data {

    struct PointCullingInstance {
        std::uint32_t point_mesh_batch_index =
            scene::StaticScene::INVALID_INDEX;
        scene::StaticScene::EnumPointCategory category =
            scene::StaticScene::EnumPointCategory::COUNT;
    };

    struct PointCullingBatch {
        // Range in PointCullingBuildContext::instance_order.
        scene::StaticScene::IndexRange instances;
    };

    // instance_order initially contains every source index in source order.
    // Its values index both instances and scene.point_instances.
    // A user function may reorder or remove entries, then describe contiguous
    // ranges in batches. Every batch must contain one mesh batch and category.
    struct PointCullingBuildContext {
        const scene::StaticScene& scene;
        std::span<const PointCullingInstance> instances;
        std::vector<std::uint32_t>& instance_order;
        std::vector<PointCullingBatch>& batches;
    };

    using PointCullingBuildFunction =
        void (*)(PointCullingBuildContext& context);

    struct PointCullingData {
        std::vector<PointCullingInstance> instances;
        std::vector<std::uint32_t> instance_order;
        std::vector<PointCullingBatch> batches;
    };

} // namespace fjr::render::data
