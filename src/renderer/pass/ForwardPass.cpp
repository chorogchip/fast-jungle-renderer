#include "FastJungle/renderer/pass/ForwardPass.hpp"

#include "FastJungle/dx12/PSOUtils.hpp"
#include "FastJungle/dx12/RootSignatureBuilder.hpp"
#include "FastJungle/dx12/Shader.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <iterator>

namespace fjr::render {

    namespace {

        enum class RootParameter : std::uint32_t {
            CAMERA,
            DRAW_TRANSFORM,
            DRAW_CONSTANTS,
            INSTANCES,
            MATERIALS,
            TEXTURE_BINDINGS,
            TEXTURES,
            SAMPLERS,
            COUNT,
        };

        [[nodiscard]]
        constexpr UINT root_index(RootParameter parameter) noexcept {
            return static_cast<UINT>(parameter);
        }

        [[nodiscard]]
        std::uint32_t pipeline_index(
            scene::StaticScene::EnumSubmeshFlag flags) noexcept {

            const auto value = static_cast<std::uint32_t>(flags);
            const auto double_sided = static_cast<std::uint32_t>(
                scene::StaticScene::EnumSubmeshFlag::DOUBLE_SIDED);
            const auto alpha_blended = static_cast<std::uint32_t>(
                scene::StaticScene::EnumSubmeshFlag::ALPHA_BLENDED);

            return
                ((value & double_sided) != 0 ? 1u : 0u) |
                ((value & alpha_blended) != 0 ? 2u : 0u);
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

    void ForwardPass::init(
        ID3D12Device* device,
        DXGI_FORMAT color_format,
        DXGI_FORMAT depth_format,
        std::uint32_t texture_descriptor_count,
        std::uint32_t sampler_descriptor_count) {

        dx::RootSignatureBuilder root_builder;
        root_builder.init(RootParameter::COUNT);
        root_builder.set_flags(
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

        root_builder.set_root_cbv(RootParameter::CAMERA)
            .reg(0).vis_all().add();
        root_builder.set_root_cbv(RootParameter::DRAW_TRANSFORM)
            .reg(1).vis_vertex().add();
        root_builder.set_constants(RootParameter::DRAW_CONSTANTS)
            .reg(2)
            .count(static_cast<UINT>(
                sizeof(SceneResources::DrawConstants) /
                sizeof(std::uint32_t)))
            .vis_all().add();
        root_builder.set_root_srv(RootParameter::INSTANCES)
            .reg(0).vis_vertex().add();
        root_builder.set_root_srv(RootParameter::MATERIALS)
            .reg(1).vis_pixel().add();
        root_builder.set_root_srv(RootParameter::TEXTURE_BINDINGS)
            .reg(2).vis_pixel().add();
        root_builder.set_resource_table(RootParameter::TEXTURES)
            .srv()
            .reg(3)
            .count(texture_descriptor_count)
            .flags(D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC)
            .add_range()
            .vis_pixel()
            .add();
        root_builder.set_sampler_table(RootParameter::SAMPLERS)
            .sampler()
            .reg(0)
            .count(sampler_descriptor_count)
            .add_range()
            .vis_pixel()
            .add();

        root_signature_ = root_builder.build(device);

        const std::filesystem::path shader_directory{
            FASTJUNGLE_SHADER_OUTPUT_DIR};
        dx::Shader vertex_shader;
        dx::Shader pixel_shader;
        vertex_shader.load(shader_directory / "Forward.vs.dxil");
        pixel_shader.load(shader_directory / "Forward.ps.dxil");

        const D3D12_INPUT_ELEMENT_DESC input_elements[]{
            {
                "POSITION", 0,
                DXGI_FORMAT_R32G32B32_FLOAT,
                0, 0,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0,
            },
            {
                "NORMAL", 0,
                DXGI_FORMAT_R32G32B32_FLOAT,
                0, 12,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0,
            },
            {
                "TEXCOORD", 0,
                DXGI_FORMAT_R32G32_FLOAT,
                0, 24,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0,
            },
        };

        auto base = dx::PSOUtils::default_graphics_desc();
        base.pRootSignature = root_signature_.Get();
        base.VS = vertex_shader.get_bytecode();
        base.PS = pixel_shader.get_bytecode();
        base.InputLayout = {
            input_elements,
            static_cast<UINT>(std::size(input_elements)),
        };
        base.NumRenderTargets = 1;
        base.RTVFormats[0] = color_format;
        base.DSVFormat = depth_format;

        for (std::uint32_t index = 0;
            index < PIPELINE_STATE_COUNT;
            ++index) {
            auto description = base;

            const bool double_sided = (index & 1u) != 0;
            const bool alpha_blended = (index & 2u) != 0;

            description.RasterizerState.CullMode = double_sided
                ? D3D12_CULL_MODE_NONE
                : D3D12_CULL_MODE_BACK;

            if (alpha_blended) {
                auto& blend = description.BlendState.RenderTarget[0];
                blend.BlendEnable = TRUE;
                blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
                blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
                blend.BlendOp = D3D12_BLEND_OP_ADD;
                blend.SrcBlendAlpha = D3D12_BLEND_ONE;
                blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
                blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
                description.DepthStencilState.DepthWriteMask =
                    D3D12_DEPTH_WRITE_MASK_ZERO;
            }

            pipeline_states_[index] = dx::PSOUtils::create_graphics(
                device,
                description);
        }
    }

    void ForwardPass::record(
        ID3D12GraphicsCommandList* command_list,
        const ForwardPassView& view) const {

        command_list->OMSetRenderTargets(
            1,
            &view.render_target,
            FALSE,
            &view.depth_stencil);

        const float clear_color[]{0.015f, 0.025f, 0.04f, 1.0f};
        command_list->ClearRenderTargetView(
            view.render_target,
            clear_color,
            0,
            nullptr);
        command_list->ClearDepthStencilView(
            view.depth_stencil,
            D3D12_CLEAR_FLAG_DEPTH,
            1.0f,
            0,
            0,
            nullptr);

        set_viewport(command_list, view.width, view.height);

        command_list->SetGraphicsRootSignature(root_signature_.Get());
        command_list->SetGraphicsRootDescriptorTable(
            root_index(RootParameter::TEXTURES),
            view.textures);
        command_list->SetGraphicsRootDescriptorTable(
            root_index(RootParameter::SAMPLERS),
            view.samplers);
        command_list->SetGraphicsRootShaderResourceView(
            root_index(RootParameter::MATERIALS),
            view.materials);
        command_list->SetGraphicsRootShaderResourceView(
            root_index(RootParameter::TEXTURE_BINDINGS),
            view.texture_bindings);
        command_list->SetGraphicsRootConstantBufferView(
            root_index(RootParameter::CAMERA),
            view.camera_constants);

        command_list->IASetPrimitiveTopology(
            D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->IASetVertexBuffers(0, 1, &view.vertices);
        command_list->IASetIndexBuffer(&view.indices);

        ID3D12PipelineState* current_pipeline = nullptr;
        for (const auto& draw : view.draws) {
            auto* selected_pipeline =
                pipeline_states_[pipeline_index(draw.flags)].Get();
            if (selected_pipeline != current_pipeline) {
                command_list->SetPipelineState(selected_pipeline);
                current_pipeline = selected_pipeline;
            }

            const bool point = draw.instance_kind ==
                SceneResources::InstanceKind::POINT;
            const auto transform_constants = point
                ? view.point_transform_constants
                : view.matrix_transform_constants;
            const auto instances = point
                ? view.point_instances
                : view.matrix_instances;

            command_list->SetGraphicsRootConstantBufferView(
                root_index(RootParameter::DRAW_TRANSFORM),
                transform_constants +
                    static_cast<UINT64>(
                        draw.transform_constant_index) *
                    SceneResources::CONSTANT_BUFFER_ALIGNMENT);
            command_list->SetGraphicsRoot32BitConstants(
                root_index(RootParameter::DRAW_CONSTANTS),
                static_cast<UINT>(
                    sizeof(draw.constants) /
                    sizeof(std::uint32_t)),
                &draw.constants,
                0);
            command_list->SetGraphicsRootShaderResourceView(
                root_index(RootParameter::INSTANCES),
                instances);

            command_list->DrawIndexedInstanced(
                draw.index_count,
                draw.instance_count,
                draw.first_index,
                draw.base_vertex,
                0);
        }
    }

    void ForwardPass::reset() noexcept {
        for (auto& pipeline : pipeline_states_) {
            pipeline.Reset();
        }
        root_signature_.Reset();
    }

} // namespace fjr::render
