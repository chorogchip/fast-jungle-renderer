#pragma once

#include <cstdint>
#include <filesystem>

#include "FastJungle/scene/StaticScene.hpp"

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
            const std::filesystem::path& texture_payload_path,
            std::uint64_t texture_payload_size);
    };

} // namespace fjr::scene
