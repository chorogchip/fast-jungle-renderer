#pragma once

#include "FastJungle/renderer/pass/PassView.hpp"

#include <d3d12.h>
#include <wrl.h>

#include <array>

namespace fjr::render {

    class VisibilityPass {
    public:
        void init(
            ID3D12Device* device,
            DXGI_FORMAT visibility_format,
            DXGI_FORMAT depth_format);

        void record(
            ID3D12GraphicsCommandList* command_list,
            const VisibilityPassView& view) const;

        void reset() noexcept;

    private:
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        std::array<
            Microsoft::WRL::ComPtr<ID3D12PipelineState>,
            2> pipeline_states_;
    };

} // namespace fjr::render
