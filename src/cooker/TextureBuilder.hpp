#pragma once

#include <filesystem>
#include <span>

#include "GeneratedTexture.hpp"
#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/scene/StaticTexturePayload.hpp"

namespace fjr::cooker {

    class TextureBuilder final {
    public:
        TextureBuilder() = delete;

        [[nodiscard]] static scene::StaticTexturePayload build(
            scene::StaticScene& scene,
            const std::filesystem::path& output_path,
            std::span<const GeneratedTexture> generated_textures);
    };

} // namespace fjr::cooker
