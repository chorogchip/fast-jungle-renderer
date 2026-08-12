#pragma once

#include <pxr/usd/usd/prim.h>

namespace fjr::cooker::internal {

    class SceneSpace;
    class StaticSceneDataBuilder;

    class UsdCameraBuilder final {
    public:
        UsdCameraBuilder() = delete;

        static void build(
            const pxr::UsdPrim& prim,
            const SceneSpace& scene_space,
            StaticSceneDataBuilder& scene_builder);
    };

} // namespace fjr::cooker::internal
