#pragma once

#include <array>
#include <cstdint>

#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/dx12/CommandQueue.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/dx12/DeviceUtils.hpp"
#include "FastJungle/dx12/SwapChain.hpp"
#include "FastJungle/dx12/Texture.hpp"
#include "FastJungle/renderer/Camera.hpp"

namespace fjr::render {

    class RendererBase {
    public:
        RendererBase() = default;
        ~RendererBase() = default;

        RendererBase(const RendererBase&) = delete;
        RendererBase(RendererBase&&) = delete;
        RendererBase& operator=(const RendererBase&) = delete;
        RendererBase& operator=(RendererBase&&) = delete;

        void init(
            void* window,
            uint32_t width, uint32_t height,
            bool vsync);

        void reset();

        void resize(uint32_t width, uint32_t height);

        Camera camera;

    protected:
        static constexpr uint32_t FRAME_COUNT = 2;

        Microsoft::WRL::ComPtr<ID3D12Device> device_;

        dx::DescriptorHeap heap_srv_cbv_uav_;
        dx::DescriptorHeap heap_cpu_srv_cbv_uav_;
        dx::DescriptorHeap heap_sampler_;
        dx::DescriptorHeap heap_dsv_;
        dx::DescriptorHeap heap_rtv_;

        dx::CommandQueue command_queue_;
        std::array<dx::CommandContext, FRAME_COUNT> command_contexts_;

        dx::SwapChain swap_chain_;
        dx::DescAlloc desc_rtv_;

        dx::Texture buffer_depth_;
        dx::DescAlloc desc_dsv_;

    private:

        void create_size_dependent_resources(uint32_t width, uint32_t height);
    };

} // namespace fjr::render
