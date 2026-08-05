#pragma once

#include "FastJungle/renderer/SceneRenderData.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render {

    class SceneDrawBuilder final {
    public:
        SceneDrawBuilder() = delete;

        [[nodiscard]]
        static SceneRenderData build(const scene::StaticScene& scene);
    };

} // namespace fjr::render
