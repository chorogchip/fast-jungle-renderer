#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>

#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::scene {

    struct StaticScenePayloadRange final {
        std::uint64_t file_offset = 0;
        std::uint64_t size = 0;
    };

    struct StaticSceneMetadata final {
        std::unique_ptr<StaticScene> scene;
        StaticScenePayloadRange texture_payload;
    };

    class StaticSceneReader final {
    public:
        StaticSceneReader() = delete;

        [[nodiscard]]
        static std::unique_ptr<StaticScene> load(
            const std::filesystem::path& path);

        [[nodiscard]]
        static StaticSceneMetadata load_metadata(
            const std::filesystem::path& path);
    };

} // namespace fjr::scene
