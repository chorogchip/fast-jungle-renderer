#pragma once

#include <cstdint>

#include "FastJungle/renderer/data/PointCullingData.hpp"

namespace fjr::render {

    enum class LodSelectionMode : uint8_t {
        AUTOMATIC, FINEST, COARSEST,
    };

    struct ObjectCategoryOptions {
        bool river_seedling = true;
        bool river_forest = true;
        bool pyramid_moss = true;
        bool other_foliage = true;
        bool terrain = true;
        bool other = true;
    };

    struct RendererOptions {
        LodSelectionMode lod_selection = LodSelectionMode::AUTOMATIC;
        ObjectCategoryOptions objects;
        bool frame_entire_scene = false;
        bool vsync = true;
    };

} // namespace fjr::render
