#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "FastJungle/renderer/RendererBase.hpp"
#include "FastJungle/renderer/RendererOptions.hpp"
#include "FastJungle/renderer/pass/ForwardPass.hpp"
#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/data/SceneDraws.hpp"
#include "FastJungle/renderer/data/SceneFrameResources.hpp"
#include "FastJungle/renderer/data/SceneResources.hpp"
#include "FastJungle/renderer/data/DynamicSceneData.hpp"
#include "FastJungle/renderer/data/FrameConstData.hpp"

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
            std::uint32_t width,
            std::uint32_t height,
            const scene::StaticScene& scene,
            const RendererOptions& options = {});

        void resize(
            std::uint32_t width,
            std::uint32_t height);

        void render();

        bool to_close() const { return false; }

    private:
        ForwardPass forward_pass_;

        RendererOptions options_;
        scene::StaticScene::EnvironmentLight environment_light_;

        data::SceneDraws scene_draws_;
        std::unique_ptr<data::SceneResources> scene_resources_;
        std::array<data::SceneFrameResources, FRAME_COUNT>
            scene_frame_resources_;
        data::DynamicSceneData dynamic_scene_data_;
        std::array<data::FrameConstData, FRAME_COUNT> frame_const_data_;
    };

} // namespace fjr
