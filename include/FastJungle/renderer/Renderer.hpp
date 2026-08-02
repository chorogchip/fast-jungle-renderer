#pragma once

#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/dx12/CommandQueue.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/dx12/DeviceUtils.hpp"
#include "FastJungle/dx12/PSOUtils.hpp"
#include "FastJungle/dx12/RootSignatureBuilder.hpp"
#include "FastJungle/dx12/Shader.hpp"
#include "FastJungle/dx12/SwapChain.hpp"

#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>

#include <cstdint>

namespace fjr {

    class Renderer {
    public:
        Renderer() = default;
        ~Renderer() = default;

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        void init(
            void* native_window,
            std::uint32_t width,
            std::uint32_t height);

        void resize(
            std::uint32_t width,
            std::uint32_t height);

        void render();
        void close();

    private:
        static constexpr UINT FRAME_COUNT = 2;

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

        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state_;
    };

} // namespace fjr
