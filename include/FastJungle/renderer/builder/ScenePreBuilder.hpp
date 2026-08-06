#pragma once

#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render {

    class ScenePreBuilder final {
    public:
        ScenePreBuilder() = delete;

        static void build(scene::StaticScene& scene) noexcept;
    };

} // namespace fjr::render
