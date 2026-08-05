#pragma once

#include "FastJungle/cooker/StaticSceneBuilder.hpp"

#include <pxr/usd/usd/stage.h>

namespace fjr::cooker::internal {

    // Application service for the Intel Jungle USD profile. It owns the
    // compilation order while the domain compilers own representation details.
    class JungleSceneCompiler final {
    public:
        [[nodiscard]] static StaticSceneBuild compile(
            pxr::UsdStageRefPtr stage);
    };

} // namespace fjr::cooker::internal
