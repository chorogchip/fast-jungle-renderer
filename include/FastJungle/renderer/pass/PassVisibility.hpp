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
            ID3D12Resource* indirect_mesh_dispatches = nullptr;
            ID3D12Resource* indirect_mesh_dispatch_count = nullptr;
            D3D12_GPU_VIRTUAL_ADDRESS visible_instances = 0;
        };

        struct PassVisibilityResources final {
            std::vector<PassVisibilityFrameResources> frames{};
            dx::DescAlloc textures{};
            dx::DescAlloc samplers{};
            D3D12_VERTEX_BUFFER_VIEW opaque_vertices{};
            D3D12_VERTEX_BUFFER_VIEW alpha_vertices{};
            D3D12_INDEX_BUFFER_VIEW indices{};
            D3D12_GPU_VIRTUAL_ADDRESS instances = 0;
            D3D12_GPU_VIRTUAL_ADDRESS vertex_decode_params = 0;
            D3D12_GPU_VIRTUAL_ADDRESS submeshes = 0;
            D3D12_GPU_VIRTUAL_ADDRESS raster_clusters = 0;
            D3D12_GPU_VIRTUAL_ADDRESS raster_cluster_vertices = 0;
            D3D12_GPU_VIRTUAL_ADDRESS raster_cluster_triangles = 0;
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
            UINT width,
            UINT height);

    private:
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        Microsoft::WRL::ComPtr<ID3D12CommandSignature> command_signature_;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> mesh_root_signature_;
        Microsoft::WRL::ComPtr<ID3D12CommandSignature>
            mesh_command_signature_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> opaque_pipeline_state_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> mesh_pipeline_state_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> river_pipeline_state_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> alpha_pipeline_state_;

        PassVisibilityResources resources_{};
    };

} // namespace fjr::render
