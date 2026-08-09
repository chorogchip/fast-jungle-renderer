#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

#include "FastJungle/cooker/GeneratedTexture.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::cooker {

    struct TextureCookOptions final {
        std::uint64_t maximum_decoded_texture_bytes =
            4ull * 1024ull * 1024ull * 1024ull;
        bool fast_bc7 = true;
    };

    class TextureCooker final {
    public:
        TextureCooker() = delete;

        [[nodiscard]]
        static std::uint64_t cook(
            scene::StaticScene& scene,
            const std::filesystem::path& payload_path,
            TextureCookOptions options = {},
            std::span<const GeneratedTexture> generated_textures = {});

        [[nodiscard]]
        static std::uint64_t reuse(
            scene::StaticScene& scene,
            const std::filesystem::path& texture_path);
    };

} // namespace fjr::cooker
