#pragma once

#include <d3d12.h>
#include <wrl.h>

#include <array>
#include <cstdint>
#include <span>

#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/dx12/View.hpp"
#include "FastJungle/renderer/data/RenderTypesDraw.hpp"

namespace fjr::render {

    class ForwardPass {

    public:
        struct ViewForRecord {
            // cpu view
            D3D12_VERTEX_BUFFER_VIEW view_vertices{};
            D3D12_INDEX_BUFFER_VIEW view_indices{};

            // cpu desc
            D3D12_CPU_DESCRIPTOR_HANDLE desc_rtv{};
            D3D12_CPU_DESCRIPTOR_HANDLE desc_dsv{};

            // root const buffer
            D3D12_GPU_VIRTUAL_ADDRESS cbuf_camera{};
            // root srv
            D3D12_GPU_VIRTUAL_ADDRESS desc_instances_matrix{};
            D3D12_GPU_VIRTUAL_ADDRESS desc_instances_point{};
            D3D12_GPU_VIRTUAL_ADDRESS desc_draw_metadata{};
            D3D12_GPU_VIRTUAL_ADDRESS desc_point_mesh_batches{};
            D3D12_GPU_VIRTUAL_ADDRESS desc_materials{};
            D3D12_GPU_VIRTUAL_ADDRESS desc_texture_bindings{};

            // no root table single view
            // D3D12_DESCRIPTOR_GPU_HANDLE ...

            // root table multiple views
            dx::DescAlloc descs_textures{};
            dx::DescAlloc descs_samplers{};

            UINT width;
            UINT height;
        };

        ViewForRecord views{};

        void init(ID3D12Device* device,
            UINT texture_descriptor_count,
            UINT sampler_descriptor_count);

        void record(
            dx::CommandContext& context,
            std::span<const data::DrawFinalCPU> draws);

    private:
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        std::array<
            std::array<
                Microsoft::WRL::ComPtr<ID3D12PipelineState>,
                data::Consts::RASTER_CLASS_CNT>,
            data::Consts::INSTANCE_KIND_CNT> pipeline_states_;
    };

} // namespace fjr::render
