#include "FastJungle/dx12/PSOUtils.hpp"

#include "FastJungle/dx12/WindowsUtils.hpp"

#include <limits>

namespace fjr::dx {

    D3D12_GRAPHICS_PIPELINE_STATE_DESC
        PSOUtils::default_graphics_desc() noexcept {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};

        description.BlendState.AlphaToCoverageEnable = FALSE;
        description.BlendState.IndependentBlendEnable = FALSE;

        for (auto& render_target : description.BlendState.RenderTarget) {
            render_target.BlendEnable = FALSE;
            render_target.LogicOpEnable = FALSE;

            render_target.SrcBlend = D3D12_BLEND_ONE;
            render_target.DestBlend = D3D12_BLEND_ZERO;
            render_target.BlendOp = D3D12_BLEND_OP_ADD;

            render_target.SrcBlendAlpha = D3D12_BLEND_ONE;
            render_target.DestBlendAlpha = D3D12_BLEND_ZERO;
            render_target.BlendOpAlpha = D3D12_BLEND_OP_ADD;

            render_target.LogicOp = D3D12_LOGIC_OP_NOOP;
            render_target.RenderTargetWriteMask =
                D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        description.SampleMask =
            std::numeric_limits<UINT>::max();

        description.RasterizerState.FillMode =
            D3D12_FILL_MODE_SOLID;
        description.RasterizerState.CullMode =
            D3D12_CULL_MODE_BACK;
        description.RasterizerState.FrontCounterClockwise =
            FALSE;
        description.RasterizerState.DepthBias = 0;
        description.RasterizerState.DepthBiasClamp = 0.0f;
        description.RasterizerState.SlopeScaledDepthBias = 0.0f;
        description.RasterizerState.DepthClipEnable = TRUE;
        description.RasterizerState.MultisampleEnable = FALSE;
        description.RasterizerState.AntialiasedLineEnable = FALSE;
        description.RasterizerState.ForcedSampleCount = 0;
        description.RasterizerState.ConservativeRaster =
            D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        const D3D12_DEPTH_STENCILOP_DESC stencil_operation{
            D3D12_STENCIL_OP_KEEP,
            D3D12_STENCIL_OP_KEEP,
            D3D12_STENCIL_OP_KEEP,
            D3D12_COMPARISON_FUNC_ALWAYS
        };

        description.DepthStencilState.DepthEnable = TRUE;
        description.DepthStencilState.DepthWriteMask =
            D3D12_DEPTH_WRITE_MASK_ALL;
        description.DepthStencilState.DepthFunc =
            D3D12_COMPARISON_FUNC_LESS;
        description.DepthStencilState.StencilEnable = FALSE;
        description.DepthStencilState.StencilReadMask =
            D3D12_DEFAULT_STENCIL_READ_MASK;
        description.DepthStencilState.StencilWriteMask =
            D3D12_DEFAULT_STENCIL_WRITE_MASK;
        description.DepthStencilState.FrontFace =
            stencil_operation;
        description.DepthStencilState.BackFace =
            stencil_operation;

        description.IBStripCutValue =
            D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

        description.PrimitiveTopologyType =
            D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        description.NumRenderTargets = 0;
        description.DSVFormat = DXGI_FORMAT_UNKNOWN;

        description.SampleDesc.Count = 1;
        description.SampleDesc.Quality = 0;

        description.NodeMask = 0;
        description.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        return description;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC
        PSOUtils::default_compute_desc() noexcept {
        D3D12_COMPUTE_PIPELINE_STATE_DESC description{};

        description.NodeMask = 0;
        description.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        return description;
    }

    Microsoft::WRL::ComPtr<ID3D12PipelineState>
        PSOUtils::create_graphics(
            ID3D12Device* device,
            const D3D12_GRAPHICS_PIPELINE_STATE_DESC& description) {

        Microsoft::WRL::ComPtr<ID3D12PipelineState>
            pipeline_state;

        abort_failed(device->CreateGraphicsPipelineState(
            &description,
            IID_PPV_ARGS(
                pipeline_state.ReleaseAndGetAddressOf())));

        return pipeline_state;
    }

    Microsoft::WRL::ComPtr<ID3D12PipelineState>
        PSOUtils::create_compute(
            ID3D12Device* device,
            const D3D12_COMPUTE_PIPELINE_STATE_DESC& description) {

        Microsoft::WRL::ComPtr<ID3D12PipelineState>
            pipeline_state;

        abort_failed(device->CreateComputePipelineState(
            &description,
            IID_PPV_ARGS(
                pipeline_state.ReleaseAndGetAddressOf())));

        return pipeline_state;
    }

}  // namespace fjr::dx