#include "FastJungle/renderer/pass/PassVisibility.hpp"

#include <array>
#include <filesystem>

#include "FastJungle/dx12/PSOUtils.hpp"
#include "FastJungle/dx12/RootSignatureBuilder.hpp"
#include "FastJungle/dx12/Shader.hpp"
#include "FastJungle/dx12/WindowsUtils.hpp"

namespace fjr::render {

    namespace {

        enum class RootParameter : std::uint32_t {
            CAMERA,
            DRAW_CONSTANTS,
            VISIBLE_INSTANCES,
            INSTANCES,
            VERTEX_DECODE_PARAMS,
            COUNT,
        };

        [[nodiscard]]
        UINT buffer_size(const dx::Buffer& buffer) noexcept {
            return static_cast<UINT>(buffer->GetDesc().Width);
        }

    } // namespace

    void PassVisibility::init(
        ID3D12Device* device,
        dx::DescriptorHeap& heap_srv_cbv_uav,
        dx::DescriptorHeap& heap_rtv,
        uint32_t indirect_draw_capacity_per_class,
        UINT width,
        UINT height) {

        indirect_draw_capacity_per_class_ = indirect_draw_capacity_per_class;
        descriptors_ = heap_srv_cbv_uav.alloc(2);
        rtv_ = heap_rtv.alloc();

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
        root_builder.set_root_srv(RootParameter::VISIBLE_INSTANCES)
            .reg(0).vis_vertex().add();
        root_builder.set_root_srv(RootParameter::INSTANCES)
            .reg(1).vis_vertex().add();
        root_builder.set_root_srv(RootParameter::VERTEX_DECODE_PARAMS)
            .reg(2).vis_vertex().add();

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
        vertex_shader.load(
            shader_directory / "visibility" / "VisibilityOpaque.vs.dxil");
        opaque_pixel_shader.load(
            shader_directory / "visibility" / "VisibilityOpaque.ps.dxil");

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

        create_buffer(device, width, height);
    }

    void PassVisibility::resize(
        ID3D12Device* device,
        UINT width,
        UINT height) {

        create_buffer(device, width, height);
    }

    void PassVisibility::create_buffer(
        ID3D12Device* device,
        UINT width,
        UINT height) {

        visibility_buffer_.reset();

        D3D12_RESOURCE_DESC description{};
        description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        description.Width = width;
        description.Height = height;
        description.DepthOrArraySize = 1;
        description.MipLevels = 1;
        description.Format = DXGI_FORMAT_R32G32_UINT;
        description.SampleDesc.Count = 1;
        description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        description.Flags =
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET |
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        visibility_buffer_.init(
            device,
            description,
            dx::TextureType::texture2d,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        visibility_buffer_.create_rtv(
            device,
            rtv_.get_cpu(),
            0, 0, 1,
            DXGI_FORMAT_R32G32_UINT);

        visibility_buffer_.create_srv(
            device,
            descriptors_.get_cpu(SRV_OFFSET),
            dx::TextureViewRange{
                .first_mip = 0,
                .mip_count = 1,
                .first_slice = 0,
                .slice_count = 1,
            },
            DXGI_FORMAT_R32G32_UINT);

        visibility_buffer_.create_uav(
            device,
            descriptors_.get_cpu(UAV_OFFSET),
            0, 0, 1,
            DXGI_FORMAT_R32G32_UINT);
    }

    void PassVisibility::record(
        dx::CommandContext& context,
        const data::DataPersistent& persistent,
        const data::DataPerFrame& frame,
        D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil,
        UINT width,
        UINT height) {

        auto* command_list = context.get();

        visibility_buffer_.transition(
            command_list,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        constexpr std::array<UINT, 4> clear_value{
            0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
        command_list->ClearUnorderedAccessViewUint(
            descriptors_.get_gpu(UAV_OFFSET),
            descriptors_.get_cpu(UAV_OFFSET),
            visibility_buffer_.get(),
            clear_value.data(),
            0,
            nullptr);

        visibility_buffer_.transition(
            command_list,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        const auto render_target = rtv_.get_cpu();
        command_list->OMSetRenderTargets(
            1, &render_target, FALSE, &depth_stencil);
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
            frame.camera.get_address());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParameter::VISIBLE_INSTANCES),
            frame.visible_instance->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParameter::INSTANCES),
            persistent.instance_transform->GetGPUVirtualAddress());
        command_list->SetGraphicsRootShaderResourceView(
            static_cast<UINT>(RootParameter::VERTEX_DECODE_PARAMS),
            persistent.vertex_decode_params->GetGPUVirtualAddress());

        const D3D12_VERTEX_BUFFER_VIEW vertex_view{
            .BufferLocation = persistent.vertex_opaque_visibility->GetGPUVirtualAddress(),
            .SizeInBytes = buffer_size(persistent.vertex_opaque_visibility),
            .StrideInBytes = sizeof(data::DataPersistent::OpaqueVertex0),
        };
        const D3D12_INDEX_BUFFER_VIEW index_view{
            .BufferLocation = persistent.index->GetGPUVirtualAddress(),
            .SizeInBytes = buffer_size(persistent.index),
            .Format = DXGI_FORMAT_R32_UINT,
        };

        command_list->IASetPrimitiveTopology(
            D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list->IASetVertexBuffers(0, 1, &vertex_view);
        command_list->IASetIndexBuffer(&index_view);

        const UINT64 command_region_size =
            static_cast<UINT64>(indirect_draw_capacity_per_class_) *
            sizeof(data::DataPerFrame::IndirectGPUDraw);

        const auto execute_indirect =
            [&](data::EnumRasterClass raster_class) {
                const auto index = static_cast<std::uint32_t>(raster_class);
                command_list->ExecuteIndirect(
                    command_signature_.Get(),
                    indirect_draw_capacity_per_class_,
                    frame.indirect_gpu_draw.get(),
                    static_cast<UINT64>(index) * command_region_size,
                    frame.indirect_gpu_draw_counts.get(),
                    static_cast<UINT64>(index) * sizeof(std::uint32_t));
            };

        command_list->SetPipelineState(opaque_pipeline_state_.Get());
        execute_indirect(data::EnumRasterClass::PYRAMID);
        execute_indirect(data::EnumRasterClass::TERRAIN);
        execute_indirect(data::EnumRasterClass::OPAQUE_SINGLE_SIDED);
        execute_indirect(data::EnumRasterClass::RIVER);

        visibility_buffer_.transition(
            command_list,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }

} // namespace fjr::render
