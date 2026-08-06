#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::scene {

    struct StaticScenePayloadRange final {
        std::filesystem::path path;
        std::uint64_t file_offset = 0;
        std::uint64_t size = 0;
    };

    struct StaticSceneMetadata final {
        std::unique_ptr<StaticScene> scene;
        StaticScenePayloadRange texture_payload;
    };

    struct StaticTextureMetadata final {
        std::vector<StaticScene::Char> strings;
        std::vector<StaticScene::TexturePayloadRef> texture_payload_refs;
        std::vector<StaticScene::TextureMip> texture_mips;
        std::vector<StaticScene::Texture> textures;
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

        [[nodiscard]]
        static StaticTextureMetadata load_texture_metadata(
            const std::filesystem::path& path);

        [[nodiscard]]
        static std::filesystem::path texture_path(
            const std::filesystem::path& scene_path);
    };

} // namespace fjr::scene
