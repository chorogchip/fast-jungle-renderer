#pragma once

#include "FastJungle/renderer/pass/PassView.hpp"

#include <d3d12.h>
#include <wrl.h>

namespace fjr::render {

    class VisibilityResolvePass {
    public:
        void init(
            ID3D12Device* device,
            DXGI_FORMAT color_format);

        void record(
            ID3D12GraphicsCommandList* command_list,
            const VisibilityResolvePassView& view) const;

        void reset() noexcept;

    private:
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state_;
    };

} // namespace fjr::render
