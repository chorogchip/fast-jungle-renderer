#pragma once

#include <vector>

#include "GeneratedTexture.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::cooker {

    class ImpostorBuilder final {
    public:
        ImpostorBuilder() = delete;

        [[nodiscard]] static std::vector<GeneratedTexture> build(
            scene::StaticScene& scene);
    };

} // namespace fjr::cooker
