#pragma once

#include "FastJungle/renderer/RendererOptions.hpp"
#include "FastJungle/renderer/data/SceneBounds.hpp"
#include "FastJungle/renderer/data/SceneDraws.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render {

    class SceneDrawBuilder final {
    public:
        SceneDrawBuilder() = delete;

        [[nodiscard]]
        static data::SceneDraws build(
            const scene::StaticScene& scene,
            const data::SceneBounds& bounds,
            const RendererOptions& options);
    };

} // namespace fjr::render
