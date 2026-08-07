#include "FastJungle/renderer/pass/ForwardPass.hpp"

#include <cstdint>
#include <filesystem>
#include <iterator>

#include "FastJungle/dx12/PSOUtils.hpp"
#include "FastJungle/dx12/RootSignatureBuilder.hpp"
#include "FastJungle/dx12/Shader.hpp"
#include "FastJungle/renderer/data/RenderTypesDraw.hpp"

namespace fjr::render {

    namespace {

        enum class RootParameter : std::uint32_t {
            CONSTANT_DRAW,
            ROOT_CBUF_CAMERA,
            ROOT_SRV_INSTANCES,
            ROOT_SRV_DRAW_METADATA,
            ROOT_SRV_POINT_MESH_BATCHES,
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
            .reg(1)
            .count(2)
            .vis_all().add();

        root_builder.set_root_cbv(RootParameter::ROOT_CBUF_CAMERA)
            .reg(0)
            .vis_all().add();

        root_builder.set_root_srv(RootParameter::ROOT_SRV_INSTANCES)
            .reg(0)
            .vis_vertex().add();

        root_builder.set_root_srv(RootParameter::ROOT_SRV_DRAW_METADATA)
            .reg(0)
            .space(1)
            .vis_all().add();

        root_builder.set_root_srv(RootParameter::ROOT_SRV_POINT_MESH_BATCHES)
            .reg(1)
            .space(1)
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
        dx::Shader matrix_vertex_shader{};
        dx::Shader point_vertex_shader{};
        dx::Shader pixel_shader{};
        matrix_vertex_shader.load(
            shader_directory / "ForwardMatrix.vs.dxil");
        point_vertex_shader.load(
            shader_directory / "ForwardPoint.vs.dxil");
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
        base.PS = pixel_shader.get_bytecode();
        base.InputLayout = {
            input_elements,
            static_cast<UINT>(std::size(input_elements)),
        };
        base.NumRenderTargets = 1;
        base.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        base.DSVFormat = DXGI_FORMAT_D32_FLOAT;

        for (std::uint32_t instance_kind = 0;
            instance_kind < data::Consts::INSTANCE_KIND_CNT;
            ++instance_kind) {
            const bool point = instance_kind == static_cast<std::uint32_t>(
                data::EnumInstanceKind::POINT);

            for (std::uint32_t raster_class = 0;
                raster_class < data::Consts::RASTER_CLASS_CNT;
                ++raster_class) {
                auto description = base;
                description.VS = point
                    ? point_vertex_shader.get_bytecode()
                    : matrix_vertex_shader.get_bytecode();
                description.RasterizerState.CullMode =
                    raster_class == static_cast<std::uint32_t>(
                        data::EnumRasterClass::ALPHA_TESTED_DOUBLE_SIDED)
                    ? D3D12_CULL_MODE_NONE
                    : D3D12_CULL_MODE_BACK;

                pipeline_states_[instance_kind][raster_class] =
                    dx::PSOUtils::create_graphics(
                        device,
                        description);
            }
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
            static_cast<UINT>(RootParameter::ROOT_SRV_DRAW_METADATA),
            views.desc_draw_metadata);
        context->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParameter::ROOT_SRV_POINT_MESH_BATCHES),
            views.desc_point_mesh_batches);
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

            const auto instance_kind = static_cast<std::uint32_t>(
                draw.instance_kind);
            const auto raster_class = static_cast<std::uint32_t>(
                draw.raster_class);
            auto* selected_pipeline =
                pipeline_states_[instance_kind][raster_class].Get();

            if (selected_pipeline != current_pipeline) {
                context->SetPipelineState(selected_pipeline);
                current_pipeline = selected_pipeline;
            }

            const bool point_instanced =
                draw.instance_kind == data::EnumInstanceKind::POINT;
            const auto instances = point_instanced
                ? views.desc_instances_point
                : views.desc_instances_matrix;

            context->SetGraphicsRootShaderResourceView(
                static_cast<UINT>(RootParameter::ROOT_SRV_INSTANCES),
                instances);

            const uint32_t draw_constants[] = {
                draw.draw_id,
                draw.instance_offset
            };

            context->SetGraphicsRoot32BitConstants(
                static_cast<UINT>(RootParameter::CONSTANT_DRAW),
                2, draw_constants, 0);

            context->DrawIndexedInstanced(
                draw.count_index,
                draw.count_instance,
                draw.offset_index,
                static_cast<INT>(draw.offset_vertex),
                0);
                // error fix: draw.instance_offset);
        }
    }

} // namespace fjr::render
