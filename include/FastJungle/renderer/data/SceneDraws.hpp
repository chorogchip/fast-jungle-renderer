#pragma once

#include <vector>

#include "FastJungle/renderer/data/RenderTypesDraw.hpp"

namespace fjr::render::data {

    struct SceneDraws {
        std::vector<DrawFinalGPUIndirect> draw_items;
    };

} // namespace fjr::render::data
