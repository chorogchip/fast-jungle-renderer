#pragma once

#include <cstdint>

namespace fjr::render {

    enum class LodSelectionMode : uint8_t {
        AUTOMATIC, FINEST, COARSEST,
    };

    enum class ObjectSelectionMode : uint8_t {
        DEFAULT_ALL,
        DEMO_PYRAMID,
        DEMO_FOLIAGE1,
        DEMO_BASIC,
    };

    struct RendererOptions {
        LodSelectionMode lod_selection = LodSelectionMode::AUTOMATIC;
        ObjectSelectionMode object_selection = ObjectSelectionMode::DEFAULT_ALL;
        bool frame_entire_scene = false;
    };

} // namespace fjr::render
