#pragma once

#include "FastJungle/renderer/data/PointCullingData.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render {

    class PointCullingDataBuilder final {
    public:
        PointCullingDataBuilder() = delete;

        [[nodiscard]]
        static data::PointCullingData build(
            const scene::StaticScene& scene,
            data::PointCullingBuildFunction user_build_function);
    };

} // namespace fjr::render
