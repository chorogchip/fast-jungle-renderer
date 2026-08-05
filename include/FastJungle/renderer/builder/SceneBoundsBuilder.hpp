#pragma once

#include "FastJungle/renderer/data/SceneBounds.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render {

    class SceneBoundsBuilder final {
    public:
        SceneBoundsBuilder() = delete;

        [[nodiscard]]
        static data::SceneBounds build(
            const scene::StaticScene& scene);
    };

} // namespace fjr::render