#pragma once

#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::cooker {

    class RasterClusterBuilder final {
    public:
        RasterClusterBuilder() = delete;

        static void build(scene::StaticScene& scene);
    };

} // namespace fjr::cooker
