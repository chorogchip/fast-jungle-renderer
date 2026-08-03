#pragma once

#include "FastJungle/cooker/JungleScene.hpp"

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
            const cooker::JungleScene& scene);

        [[nodiscard]]
        static cooker::JungleScene read(
            const std::filesystem::path& path);
    };

} // namespace fjr::scene
