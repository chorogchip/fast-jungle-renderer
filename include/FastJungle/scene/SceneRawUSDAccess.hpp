#pragma once

#include "FastJungle/scene/SceneRawUSD.hpp"

#if 0
#include <pxr/usd/ar/resolverContext.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/stageLoadRules.h>

namespace fjr::scene::raw {

    // Opt-in OpenUSD access for future SceneSource translation code. Keeping
    // this separate prevents OpenUSD headers from reaching regular renderer code.
    class SceneRawJungleUsdAccess {
    public:
        static const pxr::UsdStageRefPtr& stage(const SceneRawJungle& scene);
        static const pxr::SdfLayerHandle& root_layer(const SceneRawJungle& scene);
        static const pxr::SdfLayerHandleVector& used_layers(const SceneRawJungle& scene);
        static const pxr::ArResolverContext& resolver_context(const SceneRawJungle& scene);
        static const pxr::UsdStageLoadRules& load_rules(const SceneRawJungle& scene);
    };

} // namespace scene::raw

#endif