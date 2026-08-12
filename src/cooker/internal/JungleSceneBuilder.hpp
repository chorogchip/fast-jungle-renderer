#pragma once

#include "FastJungle/scene/StaticScene.hpp"

#include <pxr/usd/usd/stage.h>

#include <memory>

namespace fjr::cooker::internal {

    class JungleSceneBuilder final {
    public:
        [[nodiscard]] static std::unique_ptr<scene::StaticScene> build(
            pxr::UsdStageRefPtr stage);
    };

} // namespace fjr::cooker::internal
