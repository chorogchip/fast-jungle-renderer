#pragma once

#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include <source_location>
#include <vector>

#include "FastJungle/dx12/Texture.hpp"

namespace fjr::dx {

    class SwapChain {
    public:
        SwapChain() = default;

        SwapChain(const SwapChain&) = delete;
        SwapChain& operator=(const SwapChain&) = delete;

        SwapChain(SwapChain&&) noexcept = default;
        SwapChain& operator=(SwapChain&&) noexcept = default;

        void init(
            IDXGIFactory4* factory,
            ID3D12CommandQueue* command_queue,
            HWND hwnd,
            UINT width,
            UINT height,
            UINT frame_count,
            bool vsync,
            std::source_location loc =
                std::source_location::current());

        void present(
            std::source_location loc =
                std::source_location::current());

        void resize(
            UINT width,
            UINT height,
            std::source_location loc =
                std::source_location::current());

        [[nodiscard]] UINT get_current_frame() const noexcept {
            return current_frame_;
        }

        [[nodiscard]] dx::Texture& get_current_buffer() noexcept {
            return buffers_[current_frame_];
        }

        [[nodiscard]] dx::Texture& get_buffer(UINT index) noexcept {
            return buffers_[index];
        }

        [[nodiscard]] const dx::Texture& get_buffer(
            UINT index) const noexcept {
            return buffers_[index];
        }

        [[nodiscard]] UINT get_width() const noexcept {
            return width_;
        }

        [[nodiscard]] UINT get_height() const noexcept {
            return height_;
        }

        [[nodiscard]] UINT get_frame_count() const noexcept {
            return frame_count_;
        }

        [[nodiscard]] bool get_vsync() const noexcept {
            return vsync_;
        }

        [[nodiscard]] IDXGISwapChain3* get_swap_chain() const noexcept {
            return swap_chain_.Get();
        }

        [[nodiscard]] IDXGISwapChain3* operator->() const noexcept {
            return swap_chain_.Get();
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return swap_chain_ != nullptr;
        }

    private:
        Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain_;
        std::vector<dx::Texture> buffers_;

        UINT width_ = 0;
        UINT height_ = 0;
        UINT frame_count_ = 0;
        UINT current_frame_ = 0;

        bool vsync_ = true;
        bool tearing_supported_ = false;
    };

} // namespace fjr::dx
