#pragma once

#include <d3d12.h>

#include "FastJungle/dx12/CommandQueue.hpp"
#include "FastJungle/renderer/data/SceneResources.hpp"
#include "FastJungle/renderer/data/SceneResourcesTemp.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render {

    class SceneResourcesBuilder final {
    public:
        SceneResourcesBuilder() = delete;

        struct Context {
            ID3D12Device* device = nullptr;
            dx::CommandQueue* command_queue = nullptr;
        };

        [[nodiscard]]
        static data::SceneResources build(
            const Context& context,
            const scene::StaticScene& scene,
            const data::SceneResourcesTemp& source);
    };

} // namespace fjr::render