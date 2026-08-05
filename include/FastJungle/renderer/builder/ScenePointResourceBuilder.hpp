#pragma once

#include <span>

#include "FastJungle/renderer/data/RenderTypesDraw.hpp"
#include "FastJungle/renderer/data/SceneBounds.hpp"
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
            std::span<
            const data::DrawFinalGPUIndirect>
            draw_items);
    };

} // namespace fjr::render