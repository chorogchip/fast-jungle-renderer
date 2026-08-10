#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/renderer/data/DataPerFrame.hpp"
#include "FastJungle/renderer/data/DataPersistent.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render {

    class ForwardPass final {
    public:
        void init(
            ID3D12Device* device,
            UINT texture_descriptor_count,
            std::uint32_t indirect_draw_capacity_per_class);

        void record(
            dx::CommandContext& context,
            const data::DataPersistent& persistent,
            const data::DataPerFrame& frame,
            D3D12_GPU_VIRTUAL_ADDRESS camera_constants,
            D3D12_CPU_DESCRIPTOR_HANDLE render_target,
            D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil,
            UINT width,
            UINT height) const;

    private:
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        Microsoft::WRL::ComPtr<ID3D12CommandSignature> command_signature_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> opaque_pipeline_state_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> alpha_pipeline_state_;
        std::uint32_t indirect_draw_capacity_per_class_ = 0;
    };

} // namespace fjr::render
