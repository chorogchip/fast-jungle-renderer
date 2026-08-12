#pragma once

#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::cooker {

    class MeshLodBuilder final {
    public:
        MeshLodBuilder() = delete;

        static void build(scene::StaticScene& scene);
    };

} // namespace fjr::cooker
