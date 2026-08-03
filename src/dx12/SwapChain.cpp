#include "FastJungle/dx12/SwapChain.hpp"

#include "FastJungle/dx12/WindowsUtils.hpp"

namespace fjr::dx {

    void SwapChain::init(
        IDXGIFactory4* factory,
        ID3D12CommandQueue* command_queue,
        HWND hwnd,
        UINT width,
        UINT height,
        UINT frame_count,
        bool vsync) {

        width_ = width;
        height_ = height;
        frame_count_ = frame_count;
        vsync_ = vsync;

        DXGI_SWAP_CHAIN_DESC1 description{};
        description.Width = width_;
        description.Height = height_;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        description.BufferCount = frame_count_;
        description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        description.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

        Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;

        abort_failed(factory->CreateSwapChainForHwnd(
            command_queue,
            hwnd,
            &description,
            nullptr,
            nullptr,
            swap_chain.ReleaseAndGetAddressOf()));

        abort_failed(factory->MakeWindowAssociation(
            hwnd,
            DXGI_MWA_NO_ALT_ENTER));

        abort_failed(swap_chain.As(&swap_chain_));

        buffers_.resize(frame_count_);

        for (UINT i = 0; i < frame_count_; ++i) {
            Microsoft::WRL::ComPtr<ID3D12Resource> buf;
            abort_failed(swap_chain_->GetBuffer(
                i, IID_PPV_ARGS(buf.GetAddressOf())));
            buffers_[i].attach(
                buf.Get(),
                TextureType::texture2d,
                D3D12_RESOURCE_STATE_PRESENT);
        }

        current_frame_ = swap_chain_->GetCurrentBackBufferIndex();
    }

    void SwapChain::present() {
        const UINT sync_interval = vsync_ ? 1 : 0;
        const UINT flags = vsync_ ? 0 : DXGI_PRESENT_ALLOW_TEARING;

        abort_failed(swap_chain_->Present(sync_interval, flags));

        current_frame_ = swap_chain_->GetCurrentBackBufferIndex();
    }

    void SwapChain::resize(UINT width, UINT height) {
        if (width == 0 || height == 0) return;
        for (auto& buffer : buffers_) buffer.reset();

        abort_failed(swap_chain_->ResizeBuffers(
            frame_count_,
            width,
            height,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING));

        width_ = width;
        height_ = height;

        for (UINT i = 0; i < frame_count_; ++i) {
            Microsoft::WRL::ComPtr<ID3D12Resource> buf;
            abort_failed(swap_chain_->GetBuffer(
                i, IID_PPV_ARGS(buf.ReleaseAndGetAddressOf())));
            buffers_[i].attach(
                buf.Get(),
                TextureType::texture2d,
                D3D12_RESOURCE_STATE_PRESENT);
        }

        current_frame_ = swap_chain_->GetCurrentBackBufferIndex();
    }

} // namespace fjr::dx