#pragma once

#include <cstdint>
#include <filesystem>

#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/scene/StaticTexturePayload.hpp"

namespace fjr::scene {

    class StaticSceneWriter final {
    public:
        StaticSceneWriter() = delete;

        static void save(
            const std::filesystem::path& path,
            const StaticScene& scene);

        static void save(
            const std::filesystem::path& path,
            const StaticScene& scene,
            StaticTexturePayload texture_payload);

        [[nodiscard]]
        static std::filesystem::path texture_path(
            const std::filesystem::path& scene_path);
    };

} // namespace fjr::scene
