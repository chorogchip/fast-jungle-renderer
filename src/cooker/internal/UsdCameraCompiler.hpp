#pragma once

#include <pxr/usd/usd/prim.h>

namespace fjr::cooker::internal {

    class SceneSpace;
    class StaticSceneAssembler;

    class UsdCameraCompiler final {
    public:
        UsdCameraCompiler() = delete;

        static void compile(
            const pxr::UsdPrim& prim,
            const SceneSpace& scene_space,
            StaticSceneAssembler& assembler);
    };

} // namespace fjr::cooker::internal
