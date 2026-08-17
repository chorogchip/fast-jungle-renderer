#pragma once

#include <cstdint>
#include <vector>
#include <d3d12.h>
#include <wrl.h>

#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/dx12/View.hpp"

namespace fjr::render {

    class PassResolve final {
    public:
        struct PassResolveResources final {
            std::vector<D3D12_GPU_VIRTUAL_ADDRESS> cameras{};
            dx::DescAlloc inputs{};
            dx::DescAlloc textures{};
            dx::DescAlloc samplers{};
            dx::DescAlloc frame_buffer_uav{};
            std::vector<dx::DescAlloc> software_inputs{};
        };

        void init(
            ID3D12Device* device,
            PassResolveResources resources);

        void record(
            dx::CommandContext& context,
            uint32_t frame_index,
            UINT width,
            UINT height);

    private:
        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state_;

        PassResolveResources resources_{};
    };

} // namespace fjr::render
