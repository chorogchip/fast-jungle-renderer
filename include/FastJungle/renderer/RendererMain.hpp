#pragma once

#include <array>
#include <cstdint>

#include "FastJungle/renderer/RendererBase.hpp"
#include "FastJungle/renderer/pass/GpuCullingPass.hpp"
#include "FastJungle/renderer/pass/ForwardPass.hpp"
#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/data/DataPersistent.hpp"
#include "FastJungle/renderer/data/DataPerFrame.hpp"

namespace fjr::render {

    class RendererMain : public RendererBase {

    public:
        RendererMain() = default;
        ~RendererMain() = default;

        RendererMain(const RendererMain&) = delete;
        RendererMain(RendererMain&&) = delete;
        RendererMain& operator=(const RendererMain&) = delete;
        RendererMain& operator=(RendererMain&&) = delete;

        void init(
            void* window,
            uint32_t width, uint32_t height,
            const scene::StaticScene& scene);

        void resize(uint32_t width, uint32_t height);

        void render();

    private:
        GpuCullingPass gpu_culling_pass_;
        ForwardPass forward_pass_;

        scene::StaticScene::EnvironmentLight environment_light_;

        data::DataPersistent data_persistant_;
        std::array<data::DataPerFrame, FRAME_COUNT> data_per_frame_;
    };

} // namespace fjr
