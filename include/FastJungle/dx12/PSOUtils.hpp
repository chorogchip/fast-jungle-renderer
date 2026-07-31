#pragma once

#include <wrl.h>
#include <d3d12.h>

namespace fjr::dx {

    class PSOUtils {

    public:
        PSOUtils() = delete;

        [[nodiscard]]
        static D3D12_GRAPHICS_PIPELINE_STATE_DESC
            default_graphics_desc() noexcept;

        [[nodiscard]]
        static D3D12_COMPUTE_PIPELINE_STATE_DESC
            default_compute_desc() noexcept;

        [[nodiscard]]
        static Microsoft::WRL::ComPtr<ID3D12PipelineState>
            create_graphics(
                ID3D12Device* device,
                const D3D12_GRAPHICS_PIPELINE_STATE_DESC& description);

        [[nodiscard]]
        static Microsoft::WRL::ComPtr<ID3D12PipelineState>
            create_compute(
                ID3D12Device* device,
                const D3D12_COMPUTE_PIPELINE_STATE_DESC& description);
    };

} // namespace fjr::dx