#include "FastJungle/renderer/pass/ForwardPass.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <limits>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/PSOUtils.hpp"
#include "FastJungle/dx12/RootSignatureBuilder.hpp"
#include "FastJungle/dx12/Shader.hpp"
#include "FastJungle/dx12/WindowsUtils.hpp"

namespace fjr::render {

    namespace {

        enum class RootParameter : std::uint32_t {
            DRAW_CONSTANTS,
            CAMERA,
            VISIBLE_INSTANCES,
            INSTANCES,
            MATERIALS,
            TEXTURES,
            SAMPLERS,
            COUNT,
        };

        [[nodiscard]]
        UINT buffer_size(const dx::Buffer& buffer, const char* subject) {
            const UINT64 size = buffer->GetDesc().Width;
            if (size > std::numeric_limits<UINT>::max()) {
                log::Logger::g_logger
                    << subject << " exceeds the D3D12 view size limit."
                    << log::abrt();
            }
            return static_cast<UINT>(size);
        }

    } // namespace

    void ForwardPass::init(
        ID3D12Device* device,
        UINT texture_descriptor_count,
        UINT sampler_descriptor_count,
        std::uint32_t indirect_draw_capacity_per_class) {

        indirect_draw_capacity_per_class_ =
            std::max(indirect_draw_capacity_per_class, 1u);

        dx::RootSignatureBuilder root_builder;
        root_builder.init(RootParameter::COUNT);
        root_builder.set_flags(
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

        root_builder.set_constants(RootParameter::DRAW_CONSTANTS)
            .reg(1)
            .count(data::DataPerFrame::IndirectGPUDraw::ROOT_CONST_CNT)
            .vis_all()
            .add();
        root_builder.set_root_cbv(RootParameter::CAMERA)
            .reg(0).vis_all().add();
        root_builder.set_root_srv(RootParameter::VISIBLE_INSTANCES)
            .reg(0).vis_vertex().add();
        root_builder.set_root_srv(RootParameter::INSTANCES)
            .reg(1).vis_vertex().add();
        root_builder.set_root_srv(RootParameter::MATERIALS)
            .reg(2).vis_pixel().add();

        root_builder.set_resource_table(RootParameter::TEXTURES)
            .srv()
            .reg(3)
            .count(std::max(texture_descriptor_count, 1u))
            .flags(D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC)
            .add_range()
            .vis_pixel()
            .add();
        root_builder.set_sampler_table(RootParameter::SAMPLERS)
            .sampler()
            .reg(0)
            .count(std::max(sampler_descriptor_count, 1u))
            .add_range()
            .vis_pixel()
            .add();

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
        dx::Shader pixel_shader;
        vertex_shader.load(shader_directory / "Forward.vs.dxil");
        pixel_shader.load(shader_directory / "Forward.ps.dxil");

        const std::array<D3D12_INPUT_ELEMENT_DESC, 3> input_elements{
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "POSITION",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = 0,
                .AlignedByteOffset = 0,
                .InputSlotClass =
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "NORMAL",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = 1,
                .AlignedByteOffset = 0,
                .InputSlotClass =
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
            D3D12_INPUT_ELEMENT_DESC{
                .SemanticName = "TEXCOORD",
                .SemanticIndex = 0,
                .Format = DXGI_FORMAT_R32G32_FLOAT,
                .InputSlot = 2,
                .AlignedByteOffset = 0,
                .InputSlotClass =
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                .InstanceDataStepRate = 0,
            },
        };

        auto base = dx::PSOUtils::default_graphics_desc();
        base.pRootSignature = root_signature_.Get();
        base.VS = vertex_shader.get_bytecode();
        base.PS = pixel_shader.get_bytecode();
        base.InputLayout = {
            input_elements.data(),
            static_cast<UINT>(input_elements.size()),
        };
        base.NumRenderTargets = 1;
        base.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        base.DSVFormat = DXGI_FORMAT_D32_FLOAT;

        for (std::uint32_t raster_class = 0;
            raster_class < data::Consts::RASTER_CLASS_CNT;
            ++raster_class) {

            auto description = base;
            description.RasterizerState.CullMode =
                raster_class == static_cast<std::uint32_t>(
                    data::EnumRasterClass::ALPHA_TESTED_DOUBLE_SIDED)
                ? D3D12_CULL_MODE_NONE
                : D3D12_CULL_MODE_BACK;
            pipeline_states_[raster_class] =
                dx::PSOUtils::create_graphics(device, description);
        }
    }

    void ForwardPass::record(
        dx::CommandContext& context,
        const data::DataPersistent& persistent,
        const data::DataPerFrame& frame,
        D3D12_GPU_VIRTUAL_ADDRESS camera_constants,
        D3D12_CPU_DESCRIPTOR_HANDLE render_target,
        D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil,
        UINT width,
        UINT height) const {

        auto* command_list = context.get();

        command_list->OMSetRenderTargets(
            1, &render_target, FALSE, &depth_stencil);
        constexpr std::array<float, 4> clear_color{
            0.015f, 0.025f, 0.04f, 1.0f};
        command_list->ClearRenderTargetView(
            render_target, clear_color.data(), 0, nullptr);
        command_list->ClearDepthStencilView(
            depth_stencil,
            D3D12_CLEAR_FLAG_DEPTH,
            1.0f,
            0,
            0,
            nullptr);
        context.RSSetViewPortScissorRect(width, height);

        command_list->SetGraphicsRootSignature(root_signature_.Get());
        command_list->SetGraphicsRootConstantBufferView(
            static_cast<UINT>(RootParameter::CAMERA),
            camera_constants);
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParameter::VISIBLE_INSTANCES),
            frame.visible_instance->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParameter::INSTANCES),
            persistent.instance_transform->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParameter::MATERIALS),
            persistent.material->GetGPUVirtualAddress());
        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParameter::TEXTURES),
            persistent.texture_descriptors.get_gpu());
        command_list->SetGraphicsRootDescriptorTable(
            static_cast<UINT>(RootParameter::SAMPLERS),
            persistent.samplers.get_gpu());

        const std::array<D3D12_VERTEX_BUFFER_VIEW, 3> vertex_views{
            D3D12_VERTEX_BUFFER_VIEW{
                .BufferLocation =
                    persistent.vertex_pos->GetGPUVirtualAddress(),
                .SizeInBytes = buffer_size(
                    persistent.vertex_pos, "Position buffer"),
                .StrideInBytes = sizeof(DirectX::XMFLOAT3),
            },
            D3D12_VERTEX_BUFFER_VIEW{
                .BufferLocation =
                    persistent.vertex_normal->GetGPUVirtualAddress(),
                .SizeInBytes = buffer_size(
                    persistent.vertex_normal, "Normal buffer"),
                .StrideInBytes = sizeof(DirectX::XMFLOAT3),
            },
            D3D12_VERTEX_BUFFER_VIEW{
                .BufferLocation =
                    persistent.vertex_uv->GetGPUVirtualAddress(),
                .SizeInBytes = buffer_size(
                    persistent.vertex_uv, "UV buffer"),
                .StrideInBytes = sizeof(DirectX::XMFLOAT2),
            },
        };
        const D3D12_INDEX_BUFFER_VIEW index_view{
            .BufferLocation = persistent.index->GetGPUVirtualAddress(),
            .SizeInBytes = buffer_size(persistent.index, "Index buffer"),
            .Format = DXGI_FORMAT_R32_UINT,
        };

        command_list->IASetPrimitiveTopology(
            D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->IASetVertexBuffers(
            0,
            static_cast<UINT>(vertex_views.size()),
            vertex_views.data());
        command_list->IASetIndexBuffer(&index_view);

        const UINT64 command_region_size =
            static_cast<UINT64>(indirect_draw_capacity_per_class_) *
            sizeof(data::DataPerFrame::IndirectGPUDraw);

        for (std::uint32_t raster_class = 0;
            raster_class < data::Consts::RASTER_CLASS_CNT;
            ++raster_class) {

            command_list->SetPipelineState(
                pipeline_states_[raster_class].Get());
            command_list->ExecuteIndirect(
                command_signature_.Get(),
                indirect_draw_capacity_per_class_,
                frame.indirect_gpu_draw.get(),
                static_cast<UINT64>(raster_class) * command_region_size,
                frame.indirect_gpu_draw_counts.get(),
                static_cast<UINT64>(raster_class) * sizeof(std::uint32_t));
        }
    }

} // namespace fjr::render
