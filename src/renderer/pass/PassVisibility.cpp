#include "FastJungle/renderer/pass/PassVisibility.hpp"

#include <array>
#include <cstddef>
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
            COUNT,
        };

        enum class MeshRootParameter : uint32_t {
            CAMERA,
            DISPATCH_CONSTANTS,
            VISIBLE_INSTANCES,
            INSTANCES,
            VERTEX_DECODE_PARAMS,
            SUBMESHES,
            RASTER_CLUSTERS,
            RASTER_CLUSTER_VERTICES,
            RASTER_CLUSTER_TRIANGLES,
            OPAQUE_VERTICES,
            COUNT,
        };

        template<typename T, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type>
        struct alignas(void*) PipelineSubobject final {
            D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type = Type;
            T value{};
        };

        [[nodiscard]]
        Microsoft::WRL::ComPtr<ID3D12PipelineState> create_mesh_pipeline(
            ID3D12Device* device,
            ID3D12RootSignature* root_signature,
            D3D12_SHADER_BYTECODE mesh_shader,
            D3D12_SHADER_BYTECODE pixel_shader) {

            struct PipelineStream final {
                PipelineSubobject<
                    ID3D12RootSignature*,
                    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE>
                    root_signature;
                PipelineSubobject<
                    D3D12_SHADER_BYTECODE,
                    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS>
                    mesh_shader;
                PipelineSubobject<
                    D3D12_SHADER_BYTECODE,
                    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS>
                    pixel_shader;
                PipelineSubobject<
                    D3D12_BLEND_DESC,
                    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND>
                    blend;
                PipelineSubobject<
                    UINT,
                    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK>
                    sample_mask;
                PipelineSubobject<
                    D3D12_RASTERIZER_DESC,
                    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER>
                    rasterizer;
                PipelineSubobject<
                    D3D12_DEPTH_STENCIL_DESC,
                    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL>
                    depth_stencil;
                PipelineSubobject<
                    D3D12_PRIMITIVE_TOPOLOGY_TYPE,
                    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY>
                    primitive_topology;
                PipelineSubobject<
                    D3D12_RT_FORMAT_ARRAY,
                    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS>
                    render_target_formats;
                PipelineSubobject<
                    DXGI_FORMAT,
                    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT>
                    depth_stencil_format;
                PipelineSubobject<
                    DXGI_SAMPLE_DESC,
                    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC>
                    sample_desc;
            };

            const auto defaults = dx::PSOUtils::default_graphics_desc();
            PipelineStream stream{};
            stream.root_signature.value = root_signature;
            stream.mesh_shader.value = mesh_shader;
            stream.pixel_shader.value = pixel_shader;
            stream.blend.value = defaults.BlendState;
            stream.sample_mask.value = defaults.SampleMask;
            stream.rasterizer.value = defaults.RasterizerState;
            stream.depth_stencil.value = defaults.DepthStencilState;
            stream.primitive_topology.value =
                D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            stream.render_target_formats.value.NumRenderTargets = 1;
            stream.render_target_formats.value.RTFormats[0] =
                DXGI_FORMAT_R32G32_UINT;
            stream.depth_stencil_format.value = DXGI_FORMAT_D32_FLOAT;
            stream.sample_desc.value = {1, 0};

            Microsoft::WRL::ComPtr<ID3D12Device2> device2;
            dx::abort_failed(device->QueryInterface(
                IID_PPV_ARGS(device2.ReleaseAndGetAddressOf())));

            const D3D12_PIPELINE_STATE_STREAM_DESC description{
                .SizeInBytes = sizeof(stream),
                .pPipelineStateSubobjectStream = &stream,
            };
            Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
            dx::abort_failed(device2->CreatePipelineState(
                &description,
                IID_PPV_ARGS(pipeline.ReleaseAndGetAddressOf())));
            return pipeline;
        }

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
            .reg(0).vis_vertex().add();
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

        dx::RootSignatureBuilder mesh_root_builder;
        mesh_root_builder.init(MeshRootParameter::COUNT);
        mesh_root_builder.set_flags(
            D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS);
        mesh_root_builder.set_root_cbv(MeshRootParameter::CAMERA)
            .reg(0).vis_all().add();
        mesh_root_builder.set_constants(MeshRootParameter::DISPATCH_CONSTANTS)
            .reg(1)
            .count(data::DataPerFrame::IndirectMeshDispatch::ROOT_CONST_CNT)
            .vis_all().add();
        mesh_root_builder.set_root_srv(MeshRootParameter::VISIBLE_INSTANCES)
            .reg(0).vis_all().add();
        mesh_root_builder.set_root_srv(MeshRootParameter::INSTANCES)
            .reg(1).vis_all().add();
        mesh_root_builder.set_root_srv(MeshRootParameter::VERTEX_DECODE_PARAMS)
            .reg(2).vis_all().add();
        mesh_root_builder.set_root_srv(MeshRootParameter::SUBMESHES)
            .reg(3).vis_all().add();
        mesh_root_builder.set_root_srv(MeshRootParameter::RASTER_CLUSTERS)
            .reg(4).vis_all().add();
        mesh_root_builder.set_root_srv(
            MeshRootParameter::RASTER_CLUSTER_VERTICES)
            .reg(5).vis_all().add();
        mesh_root_builder.set_root_srv(
            MeshRootParameter::RASTER_CLUSTER_TRIANGLES)
            .reg(6).vis_all().add();
        mesh_root_builder.set_root_srv(MeshRootParameter::OPAQUE_VERTICES)
            .reg(7).vis_all().add();
        mesh_root_signature_ = mesh_root_builder.build(device);

        const std::array<D3D12_INDIRECT_ARGUMENT_DESC, 2> mesh_arguments{
            D3D12_INDIRECT_ARGUMENT_DESC{
                .Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT,
                .Constant = {
                    .RootParameterIndex = static_cast<UINT>(
                        MeshRootParameter::DISPATCH_CONSTANTS),
                    .DestOffsetIn32BitValues = 0,
                    .Num32BitValuesToSet = data::DataPerFrame::
                        IndirectMeshDispatch::ROOT_CONST_CNT,
                },
            },
            D3D12_INDIRECT_ARGUMENT_DESC{
                .Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH,
            },
        };
        const D3D12_COMMAND_SIGNATURE_DESC mesh_command_description{
            .ByteStride = sizeof(data::DataPerFrame::IndirectMeshDispatch),
            .NumArgumentDescs = static_cast<UINT>(mesh_arguments.size()),
            .pArgumentDescs = mesh_arguments.data(),
            .NodeMask = 0,
        };
        dx::abort_failed(device->CreateCommandSignature(
            &mesh_command_description,
            mesh_root_signature_.Get(),
            IID_PPV_ARGS(
                mesh_command_signature_.ReleaseAndGetAddressOf())));

        const std::filesystem::path shader_directory{
            FASTJUNGLE_SHADER_OUTPUT_DIR};
        dx::Shader vertex_shader;
        dx::Shader opaque_pixel_shader;
        dx::Shader alpha_vertex_shader;
        dx::Shader alpha_pixel_shader;
        dx::Shader mesh_shader;
        dx::Shader mesh_pixel_shader;
        vertex_shader.load(
            shader_directory / "visibility" / "VisibilityOpaque.vs.dxil");
        opaque_pixel_shader.load(
            shader_directory / "visibility" / "VisibilityOpaque.ps.dxil");
        alpha_vertex_shader.load(
            shader_directory / "visibility" / "VisibilityAlpha.vs.dxil");
        alpha_pixel_shader.load(
            shader_directory / "visibility" / "VisibilityAlpha.ps.dxil");
        mesh_shader.load(
            shader_directory / "visibility" / "VisibilityMeshOpaque.ms.dxil");
        mesh_pixel_shader.load(
            shader_directory / "visibility" / "VisibilityMeshOpaque.ps.dxil");

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

        mesh_pipeline_state_ = create_mesh_pipeline(
            device,
            mesh_root_signature_.Get(),
            mesh_shader.get_bytecode(),
            mesh_pixel_shader.get_bytecode());

    }

    void PassVisibility::record(
        dx::CommandContext& context,
        uint32_t frame_index,
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
        execute_indirect(data::EnumRasterClass::OPAQUE_SINGLE_SIDED);

        context->SetPipelineState(river_pipeline_state_.Get());
        execute_indirect(data::EnumRasterClass::RIVER);

        context->SetPipelineState(alpha_pipeline_state_.Get());
        context->IASetVertexBuffers(
            0, 1, &resources_.alpha_vertices);
        execute_indirect(data::EnumRasterClass::ALPHA_TESTED);

        context->SetGraphicsRootSignature(mesh_root_signature_.Get());
        context->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(MeshRootParameter::CAMERA),
            frame.camera);
        context->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(MeshRootParameter::VISIBLE_INSTANCES),
            frame.visible_instances);
        context->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(MeshRootParameter::INSTANCES),
            resources_.instances);
        context->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(MeshRootParameter::VERTEX_DECODE_PARAMS),
            resources_.vertex_decode_params);
        context->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(MeshRootParameter::SUBMESHES),
            resources_.submeshes);
        context->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(MeshRootParameter::RASTER_CLUSTERS),
            resources_.raster_clusters);
        context->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(MeshRootParameter::RASTER_CLUSTER_VERTICES),
            resources_.raster_cluster_vertices);
        context->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(MeshRootParameter::RASTER_CLUSTER_TRIANGLES),
            resources_.raster_cluster_triangles);
        context->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(MeshRootParameter::OPAQUE_VERTICES),
            resources_.opaque_vertices.BufferLocation);
        context->SetPipelineState(mesh_pipeline_state_.Get());
        context->ExecuteIndirect(
            mesh_command_signature_.Get(),
            resources_.indirect_draw_capacity_per_class,
            frame.indirect_mesh_dispatches,
            0,
            frame.indirect_mesh_dispatch_count,
            0);
    }

} // namespace fjr::render
