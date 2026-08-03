#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

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
            std::span<const std::string> texture_paths,
            const std::filesystem::path& payload_path,
            TextureCookOptions options = {});
    };

} // namespace fjr::cooker
