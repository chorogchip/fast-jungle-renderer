#pragma once

#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include <vector>

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
            bool vsync);

        void present();
        void resize(UINT width, UINT height);

        [[nodiscard]] UINT get_current_frame() const noexcept {
            return current_frame_;
        }

        [[nodiscard]] ID3D12Resource* get_current_buffer() const noexcept {
            if (buffers_.empty()) {
                return nullptr;
            }

            return buffers_[current_frame_].Get();
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
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> buffers_;

        UINT width_ = 0;
        UINT height_ = 0;
        UINT frame_count_ = 0;
        UINT current_frame_ = 0;

        bool vsync_ = true;
    };

} // namespace fjr::dx