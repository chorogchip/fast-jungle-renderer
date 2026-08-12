#pragma once

#include <DirectXTex.h>

#include <cstdint>
#include <filesystem>
#include <string_view>

#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::cooker {

    struct TextureSource final {
        DirectX::TexMetadata metadata{};
        DirectX::ScratchImage image;
    };

    [[nodiscard]] std::string_view texture_source_key(
        const scene::StaticScene& scene,
        uint32_t texture);

    class TextureSourceBuilder final {
    public:
        TextureSourceBuilder() = delete;

        [[nodiscard]] static TextureSource build(
            const std::filesystem::path& path);

        [[nodiscard]] static TextureSource build(
            const DirectX::ScratchImage& image,
            std::string_view key);
    };

} // namespace fjr::cooker
