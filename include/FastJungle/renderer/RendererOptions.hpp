#pragma once

#include <cstdint>

namespace fjr::render {

    enum class LodSelectionMode : std::uint8_t {
        AUTOMATIC,
        FINEST,
        COARSEST,
    };

    struct RendererOptions {
        LodSelectionMode lod_selection = LodSelectionMode::AUTOMATIC;
        bool frame_entire_scene = false;
    };

} // namespace fjr::render
