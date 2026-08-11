#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/dx12/Texture.hpp"
#include "FastJungle/renderer/data/DataPerFrame.hpp"
#include "FastJungle/renderer/data/DataPersistent.hpp"

namespace fjr::render {

    class PassResolve final {
    public:
        void init(
            ID3D12Device* device,
            dx::DescriptorHeap& heap_srv_cbv_uav,
            const data::DataPersistent& persistent,
            UINT texture_descriptor_count,
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
            D3D12_GPU_DESCRIPTOR_HANDLE visibility_buffer,
            UINT width,
            UINT height);

        [[nodiscard]]
        dx::Texture& get_frame_buffer() noexcept {
            return frame_buffer_;
        }

        [[nodiscard]]
        const dx::Texture& get_frame_buffer() const noexcept {
            return frame_buffer_;
        }

    private:
        void create_geometry_views(
            ID3D12Device* device,
            const data::DataPersistent& persistent);

        void create_frame_buffer(
            ID3D12Device* device,
            UINT width,
            UINT height);

        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state_;

        dx::DescAlloc geometry_views_;
        dx::DescAlloc frame_buffer_uav_;
        dx::Texture frame_buffer_;
    };

} // namespace fjr::render
