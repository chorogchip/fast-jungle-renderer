#include "FastJungle/renderer/pass/VisibilityResolvePass.hpp"

#include "FastJungle/dx12/PSOUtils.hpp"
#include "FastJungle/dx12/RootSignatureBuilder.hpp"
#include "FastJungle/dx12/Shader.hpp"

#include <cstdint>
#include <filesystem>

namespace fjr::render {

    namespace {

        enum class RootParameter : std::uint32_t {
            VISIBILITY,
            DRAWS,
            MATERIALS,
            COUNT,
        };

        [[nodiscard]]
        constexpr UINT root_index(RootParameter parameter) noexcept {
            return static_cast<UINT>(parameter);
        }

        void set_viewport(
            ID3D12GraphicsCommandList* command_list,
            std::uint32_t width,
            std::uint32_t height) {

            const D3D12_VIEWPORT viewport{
                0.0f,
                0.0f,
                static_cast<float>(width),
                static_cast<float>(height),
                0.0f,
                1.0f,
            };
            const D3D12_RECT scissor{
                0,
                0,
                static_cast<LONG>(width),
                static_cast<LONG>(height),
            };
            command_list->RSSetViewports(1, &viewport);
            command_list->RSSetScissorRects(1, &scissor);
        }

    } // namespace

    void VisibilityResolvePass::init(
        ID3D12Device* device,
        DXGI_FORMAT color_format) {

        dx::RootSignatureBuilder root_builder;
        root_builder.init(RootParameter::COUNT);
        root_builder.set_flags(
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

        root_builder.set_resource_table(RootParameter::VISIBILITY)
            .srv()
            .reg(0)
            .count(1)
            .flags(D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE)
            .add_range()
            .vis_pixel()
            .add();
        root_builder.set_root_srv(RootParameter::DRAWS)
            .reg(1).vis_pixel().add();
        root_builder.set_root_srv(RootParameter::MATERIALS)
            .reg(2).vis_pixel().add();

        root_signature_ = root_builder.build(device);

        const std::filesystem::path shader_directory{
            FASTJUNGLE_SHADER_OUTPUT_DIR};
        dx::Shader vertex_shader;
        dx::Shader pixel_shader;
        vertex_shader.load(
            shader_directory / "VisibilityResolve.vs.dxil");
        pixel_shader.load(
            shader_directory / "VisibilityResolve.ps.dxil");

        auto description = dx::PSOUtils::default_graphics_desc();
        description.pRootSignature = root_signature_.Get();
        description.VS = vertex_shader.get_bytecode();
        description.PS = pixel_shader.get_bytecode();
        description.InputLayout = {nullptr, 0};
        description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        description.DepthStencilState.DepthEnable = FALSE;
        description.DepthStencilState.DepthWriteMask =
            D3D12_DEPTH_WRITE_MASK_ZERO;
        description.DepthStencilState.StencilEnable = FALSE;
        description.NumRenderTargets = 1;
        description.RTVFormats[0] = color_format;
        description.DSVFormat = DXGI_FORMAT_UNKNOWN;

        pipeline_state_ = dx::PSOUtils::create_graphics(
            device,
            description);
    }

    void VisibilityResolvePass::record(
        ID3D12GraphicsCommandList* command_list,
        const VisibilityResolvePassView& view) const {

        command_list->OMSetRenderTargets(
            1,
            &view.render_target,
            FALSE,
            nullptr);

        const float clear_color[]{0.015f, 0.025f, 0.04f, 1.0f};
        command_list->ClearRenderTargetView(
            view.render_target,
            clear_color,
            0,
            nullptr);

        set_viewport(command_list, view.width, view.height);
        command_list->SetGraphicsRootSignature(root_signature_.Get());
        command_list->SetPipelineState(pipeline_state_.Get());
        command_list->SetGraphicsRootDescriptorTable(
            root_index(RootParameter::VISIBILITY),
            view.visibility);
        command_list->SetGraphicsRootShaderResourceView(
            root_index(RootParameter::DRAWS),
            view.draws);
        command_list->SetGraphicsRootShaderResourceView(
            root_index(RootParameter::MATERIALS),
            view.materials);
        command_list->IASetPrimitiveTopology(
            D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->DrawInstanced(3, 1, 0, 0);
    }

    void VisibilityResolvePass::reset() noexcept {
        pipeline_state_.Reset();
        root_signature_.Reset();
    }

} // namespace fjr::render
