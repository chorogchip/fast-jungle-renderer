#pragma once

#include <d3d12.h>

#include <array>
#include <cstdint>
#include <span>

#include "FastJungle/dx12/CommandContext.hpp"
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
            std::array<
                dx::CommandContext*,
                2>
                command_lists{};
        };

        [[nodiscard]]
        static data::SceneResources build(
            const Context& context,
            const scene::StaticScene& scene,
            const data::SceneResourcesTemp& source,
            std::span<const std::uint32_t> point_instance_order);
    };

} // namespace fjr::render
