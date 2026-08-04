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
#include "FastJungle/renderer/Camera.hpp"
#include "FastJungle/renderer/FrameData.hpp"
#include "FastJungle/renderer/SceneResources.hpp"
#include "FastJungle/renderer/SceneViewer.hpp"
#include "FastJungle/renderer/pass/ForwardPass.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render {

    class RendererMain {

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
            const scene::StaticScene& scene);

        void resize(
            std::uint32_t width,
            std::uint32_t height);

        void render();

    private:
        static constexpr std::uint32_t FRAME_COUNT = 2;

        Microsoft::WRL::ComPtr<IDXGIFactory4> factory_;
        Microsoft::WRL::ComPtr<ID3D12Device> device_;

        dx::CommandQueue command_queue_;
        std::array<dx::CommandContext, FRAME_COUNT> command_contexts_;

        dx::SwapChain swap_chain_;
        std::array<dx::DescAlloc, FRAME_COUNT> desc_rtv_;

        dx::Texture buffer_depth_;
        dx::DescAlloc desc_dsv_;

        ForwardPass forward_pass_;

        Camera camera_;
        std::array<FrameData, FRAME_COUNT> frame_data_;

        std::unique_ptr<SceneResources> scene_resources_;
        SceneViewer scene_viewer_;
    };

} // namespace fjr
