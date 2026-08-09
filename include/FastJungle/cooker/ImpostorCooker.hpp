#pragma once

#include <cstdint>
#include <vector>

#include "FastJungle/cooker/GeneratedTexture.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::cooker {

    struct ImpostorCookResult final {
        std::vector<GeneratedTexture> generated_textures;
        std::uint32_t impostor_count = 0;
    };

    class ImpostorCooker final {
    public:
        ImpostorCooker() = delete;

        // Appends only actual impostor records/cards to the scene.  When the
        // .fjtex cache is reusable, bake_images is false and the same stable
        // generated keys are emitted without touching D3D12.
        [[nodiscard]] static ImpostorCookResult cook(
            scene::StaticScene& scene,
            bool bake_images);
    };

} // namespace fjr::cooker
