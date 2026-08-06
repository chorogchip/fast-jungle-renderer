#pragma once

#include "FastJungle/renderer/data/SceneBounds.hpp"
#include "FastJungle/renderer/data/SceneDraws.hpp"
#include "FastJungle/renderer/data/SceneResourcesTemp.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render {
    class ScenePointResourceBuilder final {
    public:
        ScenePointResourceBuilder() = delete;

        [[nodiscard]]
        static data::SceneResourcesTemp::PointRenderPlan build(
            const scene::StaticScene& scene,
            const data::SceneBounds& bounds,
            const data::SceneDraws& draws);
    };
} // namespace fjr::render
