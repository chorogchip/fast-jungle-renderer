#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/dx12/CommandQueue.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/dx12/DeviceUtils.hpp"
#include "FastJungle/dx12/SwapChain.hpp"
#include "FastJungle/dx12/Texture.hpp"
#include "FastJungle/renderer/component/Camera.hpp"
#include "FastJungle/renderer/component/FrameData.hpp"
#include "FastJungle/renderer/RendererOptions.hpp"
#include "FastJungle/renderer/data/SceneResources.hpp"
#include "FastJungle/renderer/builder/SceneViewer.hpp"
#include "FastJungle/renderer/pass/ForwardPass.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render {

    namespace internal {
        class CameraController;
    }

    class RendererMain {

    public:

        RendererMain();
        ~RendererMain();

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

        void handle_key_down(uint32_t virtual_key);

        bool to_close() const { return false; }

    private:
        void create_size_dependent_resources(
            std::uint32_t width,
            std::uint32_t height);

        static constexpr std::uint32_t FRAME_COUNT = 2;

        Microsoft::WRL::ComPtr<ID3D12Device> device_;

        dx::CommandQueue command_queue_;
        std::array<dx::CommandContext, FRAME_COUNT> command_contexts_;

        dx::SwapChain swap_chain_;
        dx::DescAlloc desc_rtv_;

        dx::Texture buffer_depth_;
        dx::DescAlloc desc_dsv_;

        ForwardPass forward_pass_;

        Camera camera_;
        std::unique_ptr<internal::CameraController> camera_controller_;
        std::array<FrameData, FRAME_COUNT> frame_data_;

        RendererOptions options_;
        std::unique_ptr<SceneResources> scene_resources_;
        scene::StaticScene::EnvironmentLight environment_light_;
        SceneViewer scene_viewer_;
    };

} // namespace fjr
