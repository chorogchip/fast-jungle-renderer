#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <d3d12.h>
#include <wrl.h>

#include "FastJungle/dx12/Buffer.hpp"
#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/dx12/View.hpp"

namespace fjr::render {

    class PassSWRaster final {
    public:
        struct FrameResources final {
            D3D12_GPU_VIRTUAL_ADDRESS camera = 0;
            D3D12_GPU_VIRTUAL_ADDRESS visible_instances = 0;
            ID3D12Resource* batches = nullptr;
            ID3D12Resource* batch_count = nullptr;
        };

        struct Resources final {
            std::vector<FrameResources> frames{};
            D3D12_GPU_VIRTUAL_ADDRESS instances = 0;
            D3D12_GPU_VIRTUAL_ADDRESS vertex_decode_params = 0;
            D3D12_GPU_VIRTUAL_ADDRESS submeshes = 0;
            D3D12_GPU_VIRTUAL_ADDRESS raster_clusters = 0;
            D3D12_GPU_VIRTUAL_ADDRESS raster_cluster_vertices = 0;
            D3D12_GPU_VIRTUAL_ADDRESS raster_cluster_triangles = 0;
            D3D12_GPU_VIRTUAL_ADDRESS opaque_vertices = 0;
            D3D12_GPU_VIRTUAL_ADDRESS alpha_vertices = 0;
        };

        void init(
            ID3D12Device* device,
            dx::DescriptorHeap& shader_heap,
            dx::DescriptorHeap& cpu_heap,
            Resources resources,
            uint32_t width,
            uint32_t height);

        void resize(
            ID3D12Device* device,
            uint32_t width,
            uint32_t height);

        void clear(
            dx::CommandContext& context,
            uint32_t frame_index);

        void record(
            dx::CommandContext& context,
            uint32_t frame_index);

        [[nodiscard]] dx::Buffer& get_key(uint32_t frame_index) noexcept {
            return keys_[frame_index];
        }

        [[nodiscard]] dx::DescAlloc get_key_srv(
            uint32_t frame_index) const noexcept {
            return key_srvs_[frame_index];
        }

    private:
        static constexpr uint32_t FRAME_COUNT = 2;

        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        Microsoft::WRL::ComPtr<ID3D12CommandSignature> command_signature_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;

        Resources resources_{};
        std::array<dx::Buffer, FRAME_COUNT> keys_{};
        std::array<dx::DescAlloc, FRAME_COUNT> key_uavs_{};
        std::array<dx::DescAlloc, FRAME_COUNT> key_clear_uavs_{};
        std::array<dx::DescAlloc, FRAME_COUNT> key_srvs_{};
    };

} // namespace fjr::render
