#pragma once

#include "FastJungle/scene/JungleScene.hpp"

#include <filesystem>

namespace fjr::cooker {

    class JungleUsdImporter {
    public:
        JungleUsdImporter() = delete;

        // OpenUSD is deliberately absent from this interface. The returned
        // scene owns every value needed by later cooker and renderer work.
        [[nodiscard]]
        static scene::JungleScene import_scene(
            const std::filesystem::path& root_layer);
    };

} // namespace fjr::cooker
