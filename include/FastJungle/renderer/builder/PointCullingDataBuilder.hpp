#pragma once

#include "FastJungle/renderer/data/PointCullingData.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render {

    class PointCullingDataBuilder final {
    public:
        PointCullingDataBuilder() = delete;

        [[nodiscard]]
        static data::PointCullingData build(
            const scene::StaticScene& scene);
    };

} // namespace fjr::render
