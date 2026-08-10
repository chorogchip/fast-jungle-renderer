#include "FastJungle/renderer/RendererBase.hpp"

#include <Windows.h>

namespace fjr::render {

    void RendererBase::init(
        void* window,
        uint32_t width, uint32_t height,
        bool vsync) {

        const HWND hwnd = static_cast<HWND>(window);
        const auto factory = dx::DeviceUtils::create_factory();
        device_ = dx::DeviceUtils::create_device(factory.Get());

        command_queue_.init(
            device_.Get(),
            D3D12_COMMAND_LIST_TYPE_DIRECT);

        swap_chain_.init(
            factory.Get(),
            command_queue_.get_command_queue(),
            hwnd,
            width,
            height,
            FRAME_COUNT,
            vsync);

        heap_srv_cbv_uav_.init(
            device_.Get(),
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            2048,
            true);
        heap_sampler_.init(
            device_.Get(),
            D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
            32,
            true);
        heap_dsv_.init(
            device_.Get(),
            D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            1,
            false);
        heap_rtv_.init(
            device_.Get(),
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            FRAME_COUNT,
            false);

        desc_rtv_ = heap_rtv_.alloc(FRAME_COUNT);
        desc_dsv_ = heap_dsv_.alloc();

        for (std::uint32_t frame = 0; frame < FRAME_COUNT; ++frame) {
            command_contexts_[frame].init(
                device_.Get(),
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                frame);
        }

        create_size_dependent_resources(width, height);
    }

    void RendererBase::reset() {
        command_queue_.flush();
        heap_srv_cbv_uav_.reset();
        heap_sampler_.reset();
        heap_dsv_.reset();
        heap_rtv_.reset();
    }

    void RendererBase::resize(
        std::uint32_t width,
        std::uint32_t height) {

        command_queue_.flush();
        swap_chain_.resize(width, height);
        create_size_dependent_resources(width, height);
    }

    void RendererBase::create_size_dependent_resources(
        std::uint32_t width,
        std::uint32_t height) {

        for (uint32_t frame = 0; frame < FRAME_COUNT; ++frame) {
            swap_chain_.get_buffer(frame).create_rtv(
                device_.Get(),
                desc_rtv_.get_cpu(frame),
                0, 0, 1, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
        }

        buffer_depth_.reset();

        constexpr DXGI_FORMAT DEPTH_FORMAT = DXGI_FORMAT_D32_FLOAT;

        D3D12_RESOURCE_DESC depth_description{};
        depth_description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depth_description.Width = width;
        depth_description.Height = height;
        depth_description.DepthOrArraySize = 1;
        depth_description.MipLevels = 1;
        depth_description.Format = DEPTH_FORMAT;
        depth_description.SampleDesc.Count = 1;
        depth_description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depth_description.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clear_value{};
        clear_value.Format = DEPTH_FORMAT;
        clear_value.DepthStencil.Depth = 1.0f;

        buffer_depth_.init(
            device_.Get(),
            depth_description,
            dx::TextureType::texture2d,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clear_value);

        buffer_depth_.create_dsv(
            device_.Get(), desc_dsv_.get_cpu(),
            0, 0, 1, DEPTH_FORMAT, D3D12_DSV_FLAG_NONE);
    }

} // namespace fjr::render
