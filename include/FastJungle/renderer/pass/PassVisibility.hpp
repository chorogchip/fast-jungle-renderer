#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/dx12/Texture.hpp"
#include "FastJungle/renderer/data/DataPerFrame.hpp"
#include "FastJungle/renderer/data/DataPersistent.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render {

    class PassVisibility final {
    public:
        void init(
            ID3D12Device* device,
            dx::DescriptorHeap& heap_srv_cbv_uav,
            dx::DescriptorHeap& heap_cpu_srv_cbv_uav,
            dx::DescriptorHeap& heap_rtv,
            uint32_t texture_descriptor_count,
            uint32_t indirect_draw_capacity_per_class,
            UINT width,
            UINT height);

        void resize(
            ID3D12Device* device,
            UINT width,
            UINT height);

        void record(
            dx::CommandContext& context,
            const data::DataPersistent& persistent,
            const data::DataPerFrame& frame,
            D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil,
            UINT width,
            UINT height);

        [[nodiscard]]
        D3D12_GPU_DESCRIPTOR_HANDLE get_srv() const noexcept {
            return descriptors_.get_gpu(SRV_OFFSET);
        }

        [[nodiscard]]
        const dx::Texture& get_buffer() const noexcept {
            return visibility_buffer_;
        }

    private:
        static constexpr UINT SRV_OFFSET = 0;
        static constexpr UINT UAV_OFFSET = 1;

        void create_buffer(
            ID3D12Device* device,
            UINT width,
            UINT height);

        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        Microsoft::WRL::ComPtr<ID3D12CommandSignature> command_signature_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> opaque_pipeline_state_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> river_pipeline_state_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> alpha_pipeline_state_;

        dx::Texture visibility_buffer_;
        dx::DescAlloc descriptors_;
        dx::DescAlloc clear_uav_;
        dx::DescAlloc rtv_;

        uint32_t indirect_draw_capacity_per_class_ = 0;
    };

} // namespace fjr::render
