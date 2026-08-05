#pragma once

#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/SceneRenderData.hpp"
#include "FastJungle/renderer/RendererOptions.hpp"

namespace fjr::render {

    class SceneDrawBuilder final {
    public:
        SceneDrawBuilder() = delete;

        [[nodiscard]]
        static SceneRenderData build(
            const scene::StaticScene& scene,
            const RendererOptions& options);
    };

} // namespace fjr::render
