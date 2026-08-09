#pragma once

#include <cstdint>
#include <vector>

#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::cooker {
    struct TextureCompressionPlan final {
        std::uint32_t dxgi_format = 0;
        scene::StaticScene::EnumTextureChannel source_channel =
            scene::StaticScene::EnumTextureChannel::RGBA;
        bool isolate_source_channel = false;
        bool linearize_source_channel = false;
        bool filter_as_srgb = false;
        bool use_block_compression = true;
    };

    [[nodiscard]] std::vector<TextureCompressionPlan>
    resolve_texture_compression(const scene::StaticScene& scene);

    [[nodiscard]] scene::StaticScene::TextureBinding normalize_texture_binding(
        scene::StaticScene::TextureBinding binding,
        const TextureCompressionPlan& plan) noexcept;

} // namespace fjr::cooker
