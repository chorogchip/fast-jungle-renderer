#pragma once

#include "FastJungle/dx12/Buffer.hpp"

namespace fjr::render::data {

    struct SceneFrameResources {

        struct PointResources {
            dx::Buffer instance_bins;
            dx::Buffer bin_counts;
            dx::Buffer bin_offsets;
            dx::Buffer bin_cursors;
            dx::Buffer visible_instance_ids;
            dx::Buffer indirect_commands;
        };

        PointResources points;
    };

} // namespace fjr::render::data
