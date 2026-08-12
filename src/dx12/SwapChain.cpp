#include "FastJungle/dx12/SwapChain.hpp"

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/WindowsUtils.hpp"

#include <iomanip>
#include <sstream>

namespace fjr::dx {

    void SwapChain::init(
        IDXGIFactory4* factory,
        ID3D12CommandQueue* command_queue,
        HWND hwnd,
        UINT width,
        UINT height,
        UINT frame_count,
        bool vsync,
        std::source_location loc) {

        width_ = width;
        height_ = height;
        frame_count_ = frame_count;
        vsync_ = vsync;

        Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;
        BOOL allow_tearing = FALSE;
        tearing_supported_ =
            SUCCEEDED(factory->QueryInterface(
                IID_PPV_ARGS(factory5.ReleaseAndGetAddressOf()))) &&
            SUCCEEDED(factory5->CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                &allow_tearing,
                sizeof(allow_tearing))) &&
            allow_tearing == TRUE;

        DXGI_SWAP_CHAIN_DESC1 description{};
        description.Width = width_;
        description.Height = height_;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        description.BufferCount = frame_count_;
        description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        description.Flags = tearing_supported_
            ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
            : 0;

        Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;

        abort_failed(factory->CreateSwapChainForHwnd(
            command_queue,
            hwnd,
            &description,
            nullptr,
            nullptr,
            swap_chain.ReleaseAndGetAddressOf()),
            loc);

        abort_failed(factory->MakeWindowAssociation(
            hwnd,
            DXGI_MWA_NO_ALT_ENTER),
            loc);

        abort_failed(swap_chain.As(&swap_chain_), loc);

        buffers_.resize(frame_count_);

        for (UINT i = 0; i < frame_count_; ++i) {
            Microsoft::WRL::ComPtr<ID3D12Resource> buf;
            abort_failed(swap_chain_->GetBuffer(
                i, IID_PPV_ARGS(buf.GetAddressOf())),
                loc);
            buffers_[i].attach(
                buf.Get(),
                D3D12_RESOURCE_STATE_PRESENT);
        }

        current_frame_ = swap_chain_->GetCurrentBackBufferIndex();
    }

    void SwapChain::present(std::source_location loc) {
        const UINT sync_interval = vsync_ ? 1 : 0;
        const UINT flags = !vsync_ && tearing_supported_
            ? DXGI_PRESENT_ALLOW_TEARING
            : 0;

        // Preserve the original HRESULT location for actionable DX failures.
        const HRESULT result = swap_chain_->Present(sync_interval, flags);
        if (FAILED(result)) {
            std::ostringstream message;
            message << "DXGI Present failed with HRESULT 0x"
                << std::uppercase << std::hex << std::setw(8)
                << std::setfill('0')
                << static_cast<uint32_t>(result);
            message << ".";
            log::Logger::g_logger << log::abrt(message.str(), loc);
        }

        current_frame_ = swap_chain_->GetCurrentBackBufferIndex();
    }

    void SwapChain::resize(
        UINT width,
        UINT height,
        std::source_location loc) {
        if (width == 0 || height == 0) return;
        for (auto& buffer : buffers_) buffer.reset();

        abort_failed(swap_chain_->ResizeBuffers(
            frame_count_,
            width,
            height,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            tearing_supported_
                ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
                : 0),
            loc);

        width_ = width;
        height_ = height;

        for (UINT i = 0; i < frame_count_; ++i) {
            Microsoft::WRL::ComPtr<ID3D12Resource> buf;
            abort_failed(swap_chain_->GetBuffer(
                i, IID_PPV_ARGS(buf.ReleaseAndGetAddressOf())),
                loc);
            buffers_[i].attach(
                buf.Get(),
                D3D12_RESOURCE_STATE_PRESENT);
        }

        current_frame_ = swap_chain_->GetCurrentBackBufferIndex();
    }

} // namespace fjr::dx
