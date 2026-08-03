#pragma once

#include "FastJungle/scene/JungleScene.hpp"

#include <cstdint>
#include <filesystem>

namespace fjr::scene {

    class JungleSceneFile {
    public:
        static constexpr std::uint32_t FORMAT_VERSION = 0;

        struct WriteResult {
            std::uint64_t payload_size = 0;
            std::uint64_t payload_checksum = 0;
        };

        JungleSceneFile() = delete;

        [[nodiscard]]
        static WriteResult write(
            const std::filesystem::path& path,
            const JungleScene& scene);

        [[nodiscard]]
        static JungleScene read(
            const std::filesystem::path& path);
    };

} // namespace fjr::scene
