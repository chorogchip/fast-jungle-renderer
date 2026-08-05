#pragma once

#include <DirectXMath.h>

#include <pxr/usd/usd/prim.h>

namespace fjr::cooker::internal {

    class StaticSceneAssembler;

    class UsdEnvironmentCompiler final {
    public:
        UsdEnvironmentCompiler() = delete;

        static void compile(
            const pxr::UsdPrim& prim,
            DirectX::FXMMATRIX world_transform,
            StaticSceneAssembler& assembler);
    };

} // namespace fjr::cooker::internal
