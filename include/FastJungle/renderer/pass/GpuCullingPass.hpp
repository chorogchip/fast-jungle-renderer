#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/renderer/data/DataPerFrame.hpp"
#include "FastJungle/renderer/data/DataPersistent.hpp"

namespace fjr::render {

    class GpuCullingPass final {
    public:
        void init(
            ID3D12Device* device,
            std::uint32_t indirect_draw_capacity_per_class);

        void record(
            dx::CommandContext& context,
            const data::DataPersistent& persistent,
            data::DataPerFrame& frame) const;

    private:
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> clear_pipeline_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> count_pipeline_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> scan_pipeline_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> scatter_pipeline_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> build_pipeline_;
        std::uint32_t indirect_draw_capacity_per_class_ = 0;
    };

} // namespace fjr::render
