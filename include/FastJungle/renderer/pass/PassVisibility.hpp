#pragma once

#include <cstdint>
#include <vector>
#include <d3d12.h>
#include <wrl.h>

#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/dx12/View.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render {

    class PassVisibility final {
    public:
        struct PassVisibilityFrameResources final {
            D3D12_GPU_VIRTUAL_ADDRESS camera = 0;
            dx::DescAlloc inputs{};
            ID3D12Resource* indirect_draws = nullptr;
            ID3D12Resource* indirect_draw_counts = nullptr;
        };

        struct PassVisibilityResources final {
            std::vector<PassVisibilityFrameResources> frames{};
            dx::DescAlloc textures{};
            dx::DescAlloc samplers{};
            D3D12_VERTEX_BUFFER_VIEW opaque_vertices{};
            D3D12_VERTEX_BUFFER_VIEW alpha_vertices{};
            D3D12_INDEX_BUFFER_VIEW indices{};
            D3D12_CPU_DESCRIPTOR_HANDLE render_target{};
            D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil{};
            uint32_t indirect_draw_capacity_per_class = 0;
        };

        void init(
            ID3D12Device* device,
            PassVisibilityResources resources);

        void record(
            dx::CommandContext& context,
            uint32_t frame_index,
            D3D12_GPU_VIRTUAL_ADDRESS visibility_key,
            UINT width,
            UINT height);

    private:
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        Microsoft::WRL::ComPtr<ID3D12CommandSignature> command_signature_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> opaque_pipeline_state_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState>
            atomic_opaque_pipeline_state_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> river_pipeline_state_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> alpha_pipeline_state_;

        PassVisibilityResources resources_{};
    };

} // namespace fjr::render
