#pragma once

#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/data/SceneBounds.hpp"

namespace fjr::render {

    class SceneBoundsBuilder {

    public:
        [[nodiscard]]
        static data::SceneBounds build(const scene::StaticScene& scene);
    };

} // namespace fjr::render
