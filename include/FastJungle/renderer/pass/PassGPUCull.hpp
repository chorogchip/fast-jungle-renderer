#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

#include "FastJungle/dx12/Buffer.hpp"
#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/renderer/data/DataPerFrame.hpp"
#include "FastJungle/renderer/data/DataPersistent.hpp"

namespace fjr::render {

    class PassGPUCull final {
    public:
        void init(
            ID3D12Device* device,
            dx::DescriptorHeap& heap_uav,
            uint32_t mesh_lod_count,
            uint32_t instance_count,
            std::uint32_t indirect_draw_capacity_per_class);

        void record(
            dx::CommandContext& context,
            const data::DataPersistent& persistent,
            data::DataPerFrame& frame);

    private:
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> clear_pipeline_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> count_pipeline_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> scan_pipeline_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> scatter_pipeline_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> build_pipeline_;
        uint32_t indirect_draw_capacity_per_class_ = 0;
        dx::Buffer bin_counts_{};  // uint32_t. per meshlod
        dx::Buffer bin_offsets_{};  // uint32_t, exclusive prefix sum of bin_counts
        dx::Buffer bin_cursors_{};  // uint32_t, result of scatter
        dx::Buffer cull_results_{};  // uint16_t, per instance cullcount result cache
        dx::DescAlloc cull_result_uav_{};
    };

} // namespace fjr::render
