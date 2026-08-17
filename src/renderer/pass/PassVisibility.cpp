#include "FastJungle/renderer/pass/PassVisibility.hpp"

#include <array>
#include <filesystem>
#include <utility>

#include "FastJungle/dx12/PSOUtils.hpp"
#include "FastJungle/dx12/RootSignatureBuilder.hpp"
#include "FastJungle/dx12/Shader.hpp"
#include "FastJungle/dx12/WindowsUtils.hpp"
#include "FastJungle/renderer/data/DataPerFrame.hpp"

namespace fjr::render {

    namespace {

        enum class RootParameter : uint32_t {
            CAMERA,
            DRAW_CONSTANTS,
            INPUTS,
            TEXTURES,
            MATERIAL_SAMPLER,
            VISIBILITY_KEY,
            COUNT,
        };

    } // namespace

    void PassVisibility::init(
        ID3D12Device* device,
        PassVisibilityResources resources) {

        resources_ = std::move(resources);

        dx::RootSignatureBuilder root_builder;
        root_builder.init(RootParameter::COUNT);
        root_builder.set_flags(
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

        root_builder.set_root_cbv(RootParameter::CAMERA)
            .reg(0).vis_all().add();
        root_builder.set_constants(RootParameter::DRAW_CONSTANTS)
            .reg(1)
            .count(data::DataPerFrame::IndirectGPUDraw::ROOT_CONST_CNT)
            .vis_all().add();
        root_builder.set_resource_table(RootParameter::INPUTS)
            .srv().reg(0).count(4)
            .flags(D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE)
            .add_range()
            .vis_all().add();
        root_builder.set_resource_table(RootParameter::TEXTURES)
            .srv().reg(0).space(1)
            .count(resources_.textures.get_count())
            .flags(D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC)
            .add_range()
            .vis_pixel().add();
        root_builder.set_sampler_table(RootParameter::MATERIAL_SAMPLER)
            .sampler().reg(0).count(1).add_range()
            .vis_pixel().add();
        root_builder.set_root_uav(RootParameter::VISIBILITY_KEY)
            .reg(0).vis_pixel().add();

        root_signature_ = root_builder.build(device);

        const std::array<D3D12_INDIRECT_ARGUMENT_DESC, 2> arguments{
            D3D12_INDIRECT_ARGUMENT_DESC{
                .Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT,
                .Constant = {
                    .RootParameterIndex = static_cast<UINT>(
                        RootParameter::DRAW_CONSTANTS),
                    .DestOffsetIn32BitValues = 0,
                    .Num32BitValuesToSet =
                        data::DataPerFrame::IndirectGPUDraw::ROOT_CONST_CNT,
                },
            },
            D3D12_INDIRECT_ARGUMENT_DESC{
                .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED,
            },
        };

        const D3D12_COMMAND_SIGNATURE_DESC command_description{
            .ByteStride = sizeof(data::DataPerFrame::IndirectGPUDraw),
            .NumArgumentDescs = static_cast<UINT>(arguments.size()),
            .pArgumentDescs = arguments.data(),
            .NodeMask = 0,
        };

        dx::abort_failed(device->CreateCommandSignature(
            &command_description,
            root_signature_.Get(),
            IID_PPV_ARGS(command_signature_.ReleaseAndGetAddressOf())));

        const std::filesystem::path shader_directory{
            FASTJUNGLE_SHADER_OUTPUT_DIR};
        dx::Shader vertex_shader;
        dx::Shader opaque_pixel_shader;
        dx::Shader atomic_vertex_shader;
        dx::Shader atomic_pixel_shader;
        dx::Shader alpha_vertex_shader;
        dx::Shader alpha_pixel_shader;
        vertex_shader.load(
            shader_directory / "visibility" / "VisibilityOpaque.vs.dxil");
        opaque_pixel_shader.load(
            shader_directory / "visibility" / "VisibilityOpaque.ps.dxil");
        atomic_vertex_shader.load(
            shader_directory /
                "visibility" / "VisibilityOpaqueAtomic.vs.dxil");
        atomic_pixel_shader.load(
            shader_directory /
                "visibility" / "VisibilityOpaqueAtomic.ps.dxil");
        alpha_vertex_shader.load(
            shader_directory / "visibility" / "VisibilityAlpha.vs.dxil");
        alpha_pixel_shader.load(
            shader_directory / "visibility" / "VisibilityAlpha.ps.dxil");

        const std::array<D3D12_INPUT_ELEMENT_DESC, 1> input_elements{
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "POSITION",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R16G16B16A16_UNORM,
                .InputSlot = 0,
                .AlignedByteOffset = 0,
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
        };

        auto description = dx::PSOUtils::default_graphics_desc();
        description.pRootSignature = root_signature_.Get();
        description.VS = vertex_shader.get_bytecode();
        description.PS = opaque_pixel_shader.get_bytecode();
        description.InputLayout = {
            input_elements.data(),
            static_cast<UINT>(input_elements.size()),
        };
        description.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        description.NumRenderTargets = 1;
        description.RTVFormats[0] = DXGI_FORMAT_R32G32_UINT;
        description.DSVFormat = DXGI_FORMAT_D32_FLOAT;

        opaque_pipeline_state_ =
            dx::PSOUtils::create_graphics(device, description);

        description.VS = atomic_vertex_shader.get_bytecode();
        description.PS = atomic_pixel_shader.get_bytecode();
        description.NumRenderTargets = 0;
        description.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
        atomic_opaque_pipeline_state_ =
            dx::PSOUtils::create_graphics(device, description);

        description.VS = vertex_shader.get_bytecode();
        description.PS = opaque_pixel_shader.get_bytecode();
        description.NumRenderTargets = 1;
        description.RTVFormats[0] = DXGI_FORMAT_R32G32_UINT;
        description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        river_pipeline_state_ =
            dx::PSOUtils::create_graphics(device, description);

        const std::array<D3D12_INPUT_ELEMENT_DESC, 2> alpha_input_elements{
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "POSITION",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R16G16B16A16_UNORM,
                .InputSlot = 0,
                .AlignedByteOffset = 0,
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "TEXCOORD",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R16G16_UNORM,
                .InputSlot = 0,
                .AlignedByteOffset = 8,
                .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
        };

        description.VS = alpha_vertex_shader.get_bytecode();
        description.PS = alpha_pixel_shader.get_bytecode();
        description.InputLayout = {
            alpha_input_elements.data(),
            static_cast<UINT>(alpha_input_elements.size()),
        };
        description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        alpha_pipeline_state_ =
            dx::PSOUtils::create_graphics(device, description);

    }

    void PassVisibility::record(
        dx::CommandContext& context,
        uint32_t frame_index,
        D3D12_GPU_VIRTUAL_ADDRESS visibility_key,
        UINT width,
        UINT height) {

        const auto& frame = resources_.frames[frame_index];

        context->OMSetRenderTargets(
            1,
            &resources_.render_target,
            FALSE,
            &resources_.depth_stencil);
        context->ClearDepthStencilView(
            resources_.depth_stencil,
            D3D12_CLEAR_FLAG_DEPTH,
            1.0f,
            0,
            0,
            nullptr);
        context.RSSetViewPortScissorRect(width, height);

        context->SetGraphicsRootSignature(root_signature_.Get());
        context->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(RootParameter::CAMERA),
            frame.camera);
        context->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParameter::INPUTS),
            frame.inputs.get_gpu());
        context->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParameter::TEXTURES),
            resources_.textures.get_gpu());
        context->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParameter::MATERIAL_SAMPLER),
            resources_.samplers.get_gpu());
        context->SetGraphicsRootUnorderedAccessView(
            static_cast<UINT>(RootParameter::VISIBILITY_KEY),
            visibility_key);

        context->IASetPrimitiveTopology(
            D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetVertexBuffers(
            0, 1, &resources_.opaque_vertices);
        context->IASetIndexBuffer(&resources_.indices);

        const UINT64 command_region_size =
            static_cast<UINT64>(
                resources_.indirect_draw_capacity_per_class) *
            sizeof(data::DataPerFrame::IndirectGPUDraw);

        const auto execute_indirect =
            [&](data::EnumRasterClass raster_class) {
                const auto index = static_cast<uint32_t>(raster_class);
                context->ExecuteIndirect(
                    command_signature_.Get(),
                    resources_.indirect_draw_capacity_per_class,
                    frame.indirect_draws,
                    static_cast<UINT64>(index) * command_region_size,
                    frame.indirect_draw_counts,
                    static_cast<UINT64>(index) * sizeof(uint32_t));
            };

        context->SetPipelineState(opaque_pipeline_state_.Get());
        execute_indirect(data::EnumRasterClass::PYRAMID);
        execute_indirect(data::EnumRasterClass::TERRAIN);

        context->SetPipelineState(atomic_opaque_pipeline_state_.Get());
        execute_indirect(data::EnumRasterClass::OPAQUE_SINGLE_SIDED);

        context->SetPipelineState(river_pipeline_state_.Get());
        execute_indirect(data::EnumRasterClass::RIVER);

        context->SetPipelineState(alpha_pipeline_state_.Get());
        context->IASetVertexBuffers(
            0, 1, &resources_.alpha_vertices);
        execute_indirect(data::EnumRasterClass::ALPHA_TESTED);
    }

} // namespace fjr::render
