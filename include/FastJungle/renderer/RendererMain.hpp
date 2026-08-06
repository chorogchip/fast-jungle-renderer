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
#include "FastJungle/renderer/RendererOptions.hpp"
#include "FastJungle/renderer/pass/ForwardPass.hpp"
#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/data/SceneResourcesTemp.hpp"
#include "FastJungle/renderer/data/SceneResources.hpp"
#include "FastJungle/renderer/data/DynamicSceneData.hpp"
#include "FastJungle/renderer/data/FrameConstData.hpp"

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
            const scene::StaticScene& scene,
            const RendererOptions& options = {});

        void reset();

        void resize(
            std::uint32_t width,
            std::uint32_t height);

        void render();

        bool to_close() const { return false; }

        Camera camera;
    private:
        void create_size_dependent_resources(
            uint32_t width, uint32_t height);

        static constexpr std::uint32_t FRAME_COUNT = 2;

        Microsoft::WRL::ComPtr<ID3D12Device> device_;

        dx::DescriptorHeap heap_srv_cbv_uav_;
        dx::DescriptorHeap heap_sampler_;
        dx::DescriptorHeap heap_dsv_;
        dx::DescriptorHeap heap_rtv_;

        dx::CommandQueue command_queue_;
        std::array<dx::CommandContext, FRAME_COUNT> command_contexts_;

        dx::SwapChain swap_chain_;
        dx::DescAlloc desc_rtv_;

        dx::Texture buffer_depth_;
        dx::DescAlloc desc_dsv_;

        ForwardPass forward_pass_;

        RendererOptions options_;
        scene::StaticScene::EnvironmentLight environment_light_;

        std::unique_ptr<data::SceneResourcesTemp> scene_resources_temp_;
        std::unique_ptr<data::SceneResources> scene_resources_;
        data::DynamicSceneData dynamic_scene_data_;
        std::array<data::FrameConstData, FRAME_COUNT> frame_const_data_;
    };

} // namespace fjr
