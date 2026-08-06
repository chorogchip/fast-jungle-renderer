#pragma once

#include <filesystem>
#include <memory>
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::cooker {

struct StaticSceneBuild final {
    std::unique_ptr<scene::StaticScene> scene;
};

    class StaticSceneBuilder final {
    public:
        StaticSceneBuilder() = delete;

        [[nodiscard]]
        static StaticSceneBuild build(
            const std::filesystem::path& root_layer);
    };

} // namespace fjr::cooker
