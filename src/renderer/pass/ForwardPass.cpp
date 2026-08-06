#include "FastJungle/renderer/pass/ForwardPass.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <iterator>

#include "FastJungle/core/util/EnumUtils.hpp"
#include "FastJungle/dx12/PSOUtils.hpp"
#include "FastJungle/dx12/RootSignatureBuilder.hpp"
#include "FastJungle/dx12/Shader.hpp"
#include "FastJungle/renderer/data/RenderTypesDraw.hpp"

namespace fjr::render {

    namespace {

        enum class RootParameter : std::uint32_t {
            CONSTANT_DRAW,
            ROOT_CBUF_CAMERA,
            ROOT_CBUF_TRANSFORMS,
            ROOT_SRV_INSTANCES,
            ROOT_SRV_MATERIAL,
            ROOT_SRV_TEXTURE_BINDING,
            TABLE_TEXTURES,
            TABLE_SAMPLERS,
            COUNT,
        };
    }

    void ForwardPass::init(ID3D12Device* device,
        UINT texture_descriptor_count,
        UINT sampler_descriptor_count) {

        dx::RootSignatureBuilder root_builder{};
        root_builder.init(RootParameter::COUNT);
        root_builder.set_flags(
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);


        root_builder.set_constants(RootParameter::CONSTANT_DRAW)
            .reg(2)
            .count(3)
            .vis_all().add();

        root_builder.set_root_cbv(RootParameter::ROOT_CBUF_CAMERA)
            .reg(0)
            .vis_all().add();

        root_builder.set_root_cbv(RootParameter::ROOT_CBUF_TRANSFORMS)
            .reg(1)
            .vis_vertex().add();

        root_builder.set_root_srv(RootParameter::ROOT_SRV_INSTANCES)
            .reg(0)
            .vis_vertex().add();

        root_builder.set_root_srv(RootParameter::ROOT_SRV_MATERIAL)
            .reg(1)
            .vis_pixel().add();

        root_builder.set_root_srv(RootParameter::ROOT_SRV_TEXTURE_BINDING)
            .reg(2)
            .vis_pixel().add();

        root_builder.set_resource_table(RootParameter::TABLE_TEXTURES)
            .srv()
            .reg(3)
            .count(texture_descriptor_count)  // just fix this..
            .flags(D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC)
            .add_range()
            .vis_pixel()
            .add();

        root_builder.set_sampler_table(RootParameter::TABLE_SAMPLERS)
            .sampler()
            .reg(0)
            .count(sampler_descriptor_count)  // temp
            .add_range()
            .vis_pixel()
            .add();

        root_signature_ = root_builder.build(device);

        const std::filesystem::path shader_directory{ FASTJUNGLE_SHADER_OUTPUT_DIR };
        dx::Shader vertex_shader{};
        dx::Shader pixel_shader{};
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
        base.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        base.DSVFormat = DXGI_FORMAT_D32_FLOAT;

        for (std::uint32_t index = 0; index < PIPELINE_STATE_COUNT; ++index) {
            auto description = base;

            const bool double_sided = enm::has(index, 1u);
            const bool alpha_blended = enm::has(index, 2u);

            description.RasterizerState.CullMode =
                double_sided ? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_BACK;

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
        dx::CommandContext& context,
        std::span<const data::DrawFinalCPU> draws) {

        context->OMSetRenderTargets(
            1, &views.desc_rtv, FALSE, &views.desc_dsv);
        const float clear_color[]{ 0.015f, 0.025f, 0.04f, 1.0f };
        context->ClearRenderTargetView(
            views.desc_rtv, clear_color, 0, nullptr);
        context->ClearDepthStencilView(
            views.desc_dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f,
            0, 0, nullptr);
        context.RSSetViewPortScissorRect(views.width, views.height);


        context->SetGraphicsRootSignature(root_signature_.Get());

        context->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(RootParameter::ROOT_CBUF_CAMERA),
            views.cbuf_camera);
        context->SetGraphicsRootShaderResourceView(
        static_cast<UINT>(RootParameter::ROOT_SRV_MATERIAL),
            views.desc_materials);
        context->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParameter::ROOT_SRV_TEXTURE_BINDING),
            views.desc_texture_bindings);
        context->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParameter::TABLE_TEXTURES),
            views.descs_textures.get_gpu());
        context->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParameter::TABLE_SAMPLERS),
            views.descs_samplers.get_gpu());


        context->IASetPrimitiveTopology(
            D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetVertexBuffers(0, 1, &views.view_vertices);
        context->IASetIndexBuffer(&views.view_indices);


        ID3D12PipelineState* current_pipeline = nullptr;
        for (const auto& draw : draws) {

            const auto pipeline_index = static_cast<std::uint32_t>(
                draw.flags);
            auto* selected_pipeline = pipeline_states_[pipeline_index].Get();

            if (selected_pipeline != current_pipeline) {
                context->SetPipelineState(selected_pipeline);
                current_pipeline = selected_pipeline;
            }

            const bool point_instanced =
                draw.instnace_class ==
                data::EnumPointOrMatrix::POINT;
            const auto& transform_constants = point_instanced
                ? views.cbuf_transform_point
                : views.cbuf_transform_matrix;
            const auto instances = point_instanced
                ? views.desc_instances_point
                : views.desc_instnaces_matrix;

            context->SetGraphicsRootConstantBufferView(
                static_cast<UINT>(RootParameter::ROOT_CBUF_TRANSFORMS),
                transform_constants.get_address(
                    draw.offset_cbuf_transform));

            context->SetGraphicsRootShaderResourceView(
                static_cast<UINT>(RootParameter::ROOT_SRV_INSTANCES),
                instances);

            const std::array<std::uint32_t, 3>
                root_constants{
                    draw.constants.offset_instance,
                    draw.constants.offset_material,
                    static_cast<std::uint32_t>(
                        draw.instnace_class)
                };

            context->SetGraphicsRoot32BitConstants(
                static_cast<UINT>(RootParameter::CONSTANT_DRAW),
                static_cast<UINT>(root_constants.size()),
                root_constants.data(), 0);

            context->DrawIndexedInstanced(
                draw.count_index,
                draw.count_instance,
                draw.offset_index,
                static_cast<INT>(draw.offset_vertex),
                0);
        }
    }

} // namespace fjr::render
