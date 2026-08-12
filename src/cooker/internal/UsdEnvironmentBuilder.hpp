#pragma once

#include <DirectXMath.h>

#include <pxr/usd/usd/prim.h>

namespace fjr::cooker::internal {

    class StaticSceneDataBuilder;

    class UsdEnvironmentBuilder final {
    public:
        UsdEnvironmentBuilder() = delete;

        static void build(
            const pxr::UsdPrim& prim,
            DirectX::FXMMATRIX world_transform,
            StaticSceneDataBuilder& scene_builder);
    };

} // namespace fjr::cooker::internal
