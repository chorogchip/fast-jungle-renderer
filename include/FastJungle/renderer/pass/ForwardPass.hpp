#pragma once

#include <d3d12.h>
#include <wrl.h>

#include <array>
#include <cstdint>
#include <span>

#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/dx12/View.hpp"

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
            dx::CBufferArrayView cbuf_transform_matrix{};
            dx::CBufferArrayView cbuf_transform_point{};

            // root srv
            D3D12_GPU_VIRTUAL_ADDRESS desc_instnaces_matrix{};
            D3D12_GPU_VIRTUAL_ADDRESS desc_instances_point{};
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
            std::span<const Draw::DrawDataCpu> draws);

    private:
        static constexpr std::uint32_t PIPELINE_STATE_COUNT = 4;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, PIPELINE_STATE_COUNT> pipeline_states_;
    };

} // namespace fjr::render
