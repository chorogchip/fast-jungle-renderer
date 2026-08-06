#pragma once

#include "FastJungle/renderer/Camera.hpp"
#include "FastJungle/renderer/RendererOptions.hpp"
#include "FastJungle/renderer/data/DynamicSceneData.hpp"
#include "FastJungle/renderer/data/SceneDraws.hpp"

namespace fjr::render {

    class SceneDynamicDataBuilder final {
    public:
        SceneDynamicDataBuilder() = delete;

        static void build(
            data::DynamicSceneData& output,
            const data::SceneDraws& scene,
            const Camera& camera,
            LodSelectionMode lod_selection,
            uint32_t viewport_height);
    };

} // namespace fjr::render
