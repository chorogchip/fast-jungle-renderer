#pragma once

#include <filesystem>
#include <memory>

#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::cooker {

    class StaticSceneBuilder final {
    public:
        StaticSceneBuilder() = delete;

        [[nodiscard]]
        static std::unique_ptr<scene::StaticScene> build(
            const std::filesystem::path& root_layer);
    };

} // namespace fjr::cooker
