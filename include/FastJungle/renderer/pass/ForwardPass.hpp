#pragma once

#include "FastJungle/renderer/pass/PassView.hpp"

#include <d3d12.h>
#include <wrl.h>

#include <array>
#include <cstdint>

namespace fjr::render {

    class ForwardPass {
    public:
        void init(
            ID3D12Device* device,
            DXGI_FORMAT color_format,
            DXGI_FORMAT depth_format,
            std::uint32_t texture_descriptor_count,
            std::uint32_t sampler_descriptor_count);

        void record(
            ID3D12GraphicsCommandList* command_list,
            const ForwardPassView& view) const;

        void reset() noexcept;

    private:
        static constexpr std::uint32_t PIPELINE_STATE_COUNT = 4;

        Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
        std::array<
            Microsoft::WRL::ComPtr<ID3D12PipelineState>,
            PIPELINE_STATE_COUNT> pipeline_states_;
    };

} // namespace fjr::render
