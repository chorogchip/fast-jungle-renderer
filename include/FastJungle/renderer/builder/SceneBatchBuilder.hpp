#pragma once

#include "FastJungle/renderer/data/PointCullingData.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render {

    class SceneBatchBuilder final {
    public:
        SceneBatchBuilder() = delete;

        [[nodiscard]]
        static data::PointCullingData build(
            const scene::StaticScene& scene);
    };

} // namespace fjr::render
