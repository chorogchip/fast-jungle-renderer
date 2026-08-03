#pragma once

#include <cstdint>
#include <span>
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
    };

    class TextureCompressionPolicy final {
    public:
        TextureCompressionPolicy() = delete;

        [[nodiscard]]
        static std::vector<TextureCompressionPlan> resolve(
            const scene::StaticScene& scene);

        static void apply_binding_changes(
            scene::StaticScene& scene,
            std::span<const TextureCompressionPlan> plans);
    };

} // namespace fjr::cooker
