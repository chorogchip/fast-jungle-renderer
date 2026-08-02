#pragma once

#include "FastJungle/scene/JungleScene.hpp"

#include <filesystem>

namespace fjr::cooker {

    // OpenUSD is deliberately absent from this interface. The returned scene
    // owns every value needed by later cooker and renderer work.
    scene::JungleScene import_jungle_usd(
        const std::filesystem::path& root_layer);

} // namespace fjr::cooker
