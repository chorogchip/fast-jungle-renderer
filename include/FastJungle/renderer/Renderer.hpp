#pragma once

#include "FastJungle/scene/JungleScene.hpp"

#include "FastJungle/dx12/Buffer.hpp"
#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/dx12/CommandQueue.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/dx12/DeviceUtils.hpp"
#include "FastJungle/dx12/PSOUtils.hpp"
#include "FastJungle/dx12/RootSignatureBuilder.hpp"
#include "FastJungle/dx12/Shader.hpp"
#include "FastJungle/dx12/SwapChain.hpp"
#include "FastJungle/dx12/Texture.hpp"

#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace fjr {

    class Renderer {
    public:
        Renderer();
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        void init(
            void* native_window,
            std::uint32_t width,
            std::uint32_t height,
            const scene::JungleScene& scene);

        void resize(
            std::uint32_t width,
            std::uint32_t height);

        void render();
        void close();

    private:
        static constexpr UINT FRAME_COUNT = 2;

        struct DrawBatch;

        void init_depth_buffer();
        void build_scene_geometry(const scene::JungleScene& scene);
        void update_camera();

        HWND window_ = nullptr;
        UINT width_ = 0;
        UINT height_ = 0;
        UINT frame_index_ = 0;

        Microsoft::WRL::ComPtr<IDXGIFactory4> factory_;
        Microsoft::WRL::ComPtr<ID3D12Device> device_;

        dx::CommandQueue command_queue_;
        dx::CommandContext command_context_;
        dx::SwapChain swap_chain_;
        dx::DescriptorHeap rtv_heap_;
        dx::DescriptorHeap dsv_heap_;
        dx::Texture depth_buffer_;

        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state_;

        std::vector<std::unique_ptr<DrawBatch>> draw_batches_;
        std::array<float, 16> view_projection_{};
        std::array<float, 6> render_bounds_{};
        std::uint32_t rendered_kind_count_ = 0;
    };

} // namespace fjr
