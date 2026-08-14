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
            dx::DescAlloc inputs{};
        };

        struct Resources final {
            std::vector<FrameResources> frames{};
        };

        void init(
            ID3D12Device* device,
            Resources resources,
            uint32_t width,
            uint32_t height);

        void resize(
            ID3D12Device* device,
            uint32_t width,
            uint32_t height);

        void record(
            dx::CommandContext& context,
            uint32_t frame_index,
            uint32_t width,
            uint32_t height);

    private:
        static constexpr uint32_t FRAME_COUNT = 2;

        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        Microsoft::WRL::ComPtr<ID3D12CommandSignature> command_signature_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;

        Resources resources_{};
    };

} // namespace fjr::render
