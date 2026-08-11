#include "FastJungle/renderer/pass/PassResolve.hpp"

#include <filesystem>
#include <limits>

#include "FastJungle/dx12/PSOUtils.hpp"
#include "FastJungle/dx12/RootSignatureBuilder.hpp"
#include "FastJungle/dx12/Shader.hpp"

namespace fjr::render {

    namespace {

        constexpr UINT THREAD_COUNT_X = 16;
        constexpr UINT THREAD_COUNT_Y = 16;

        enum class RootParameter : std::uint32_t {
            CAMERA,
            INSTANCES,
            VERTEX_DECODE_PARAMS,
            GEOMETRY,
            SUBMESHES,
            VISIBILITY_BUFFER,
            MATERIALS,
            TEXTURES,
            MATERIAL_SAMPLER,
            FRAME_BUFFER,
            COUNT,
        };

        [[nodiscard]]
        UINT dispatch_groups(UINT item_count, UINT thread_count) noexcept {
            return (item_count + thread_count - 1) / thread_count;
        }

    } // namespace

    void PassResolve::init(
        ID3D12Device* device,
        dx::DescriptorHeap& heap_srv_cbv_uav,
        const data::DataPersistent& persistent,
        UINT width,
        UINT height) {

        geometry_views_ = heap_srv_cbv_uav.alloc(5);
        frame_buffer_uav_ = heap_srv_cbv_uav.alloc();

        dx::RootSignatureBuilder root_builder;
        root_builder.init(RootParameter::COUNT);
        root_builder.set_flags(D3D12_ROOT_SIGNATURE_FLAG_NONE);

        root_builder.set_root_cbv(RootParameter::CAMERA)
            .reg(0).vis_all().add();
        root_builder.set_root_srv(RootParameter::INSTANCES)
            .reg(1).vis_all().add();
        root_builder.set_root_srv(RootParameter::VERTEX_DECODE_PARAMS)
            .reg(2).vis_all().add();
        root_builder.set_resource_table(RootParameter::GEOMETRY)
            .srv().reg(3).count(5)
            .flags(D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC)
            .add_range()
            .vis_all().add();
        root_builder.set_root_srv(RootParameter::SUBMESHES)
            .reg(8).vis_all().add();
        root_builder.set_resource_table(RootParameter::VISIBILITY_BUFFER)
            .srv().reg(9).count(1).add_range()
            .vis_all().add();
        root_builder.set_root_srv(RootParameter::MATERIALS)
            .reg(10).vis_all().add();
        root_builder.set_resource_table(RootParameter::TEXTURES)
            .srv().reg(0).space(1)
            .count(std::numeric_limits<UINT>::max())
            .flags(D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC)
            .add_range()
            .vis_all().add();
        root_builder.set_sampler_table(RootParameter::MATERIAL_SAMPLER)
            .sampler().reg(0).count(1).add_range()
            .vis_all().add();
        root_builder.set_resource_table(RootParameter::FRAME_BUFFER)
            .uav().reg(0).count(1).add_range()
            .vis_all().add();

        root_signature_ = root_builder.build(device);

        const std::filesystem::path shader_directory{
            FASTJUNGLE_SHADER_OUTPUT_DIR};
        dx::Shader shader;
        shader.load(
            shader_directory / "visibility" / "ResolveOpaque.cs.dxil");

        auto description = dx::PSOUtils::default_compute_desc();
        description.pRootSignature = root_signature_.Get();
        description.CS = shader.get_bytecode();
        pipeline_state_ = dx::PSOUtils::create_compute(device, description);

        create_geometry_views(device, persistent);
        create_frame_buffer(device, width, height);
    }

    void PassResolve::resize(
        ID3D12Device* device,
        UINT width,
        UINT height) {

        create_frame_buffer(device, width, height);
    }

    void PassResolve::create_geometry_views(
        ID3D12Device* device,
        const data::DataPersistent& persistent) {

        persistent.vertex_opaque_visibility.create_typed_srv(
            device,
            geometry_views_.get_cpu(0),
            DXGI_FORMAT_R16G16B16A16_UNORM,
            0,
            static_cast<UINT>(persistent.vertex_opaque_visibility->GetDesc().Width / 8));

        persistent.vertex_opaque_shading.create_structured_srv(
            device,
            geometry_views_.get_cpu(1),
            sizeof(data::DataPersistent::OpaqueVertex1),
            0,
            static_cast<UINT>(
                persistent.vertex_opaque_shading->GetDesc().Width /
                sizeof(data::DataPersistent::OpaqueVertex1)));

        persistent.vertex_alpha_visibility.create_structured_srv(
            device,
            geometry_views_.get_cpu(2),
            sizeof(data::DataPersistent::AlphaVertex0),
            0,
            static_cast<UINT>(
                persistent.vertex_alpha_visibility->GetDesc().Width /
                sizeof(data::DataPersistent::AlphaVertex0)));

        persistent.vertex_alpha_shading.create_structured_srv(
            device,
            geometry_views_.get_cpu(3),
            sizeof(data::DataPersistent::AlphaVertex1),
            0,
            static_cast<UINT>(
                persistent.vertex_alpha_shading->GetDesc().Width /
                sizeof(data::DataPersistent::AlphaVertex1)));

        persistent.index.create_typed_srv(
            device,
            geometry_views_.get_cpu(4),
            DXGI_FORMAT_R32_UINT,
            0,
            static_cast<UINT>(persistent.index->GetDesc().Width / sizeof(std::uint32_t)));
    }

    void PassResolve::create_frame_buffer(
        ID3D12Device* device,
        UINT width,
        UINT height) {

        frame_buffer_.reset();

        D3D12_RESOURCE_DESC description{};
        description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        description.Width = width;
        description.Height = height;
        description.DepthOrArraySize = 1;
        description.MipLevels = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        description.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        frame_buffer_.init(
            device,
            description,
            dx::TextureType::texture2d,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        frame_buffer_.create_uav(
            device,
            frame_buffer_uav_.get_cpu(),
            0, 0, 1,
            DXGI_FORMAT_R8G8B8A8_UNORM);
    }

    void PassResolve::record(
        dx::CommandContext& context,
        const data::DataPersistent& persistent,
        const data::DataPerFrame& frame,
        D3D12_GPU_DESCRIPTOR_HANDLE visibility_buffer,
        UINT width,
        UINT height) {

        auto* command_list = context.get();

        frame_buffer_.transition(
            command_list,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        // The resolve UAV stores display-encoded values. This is the sRGB
        // encoding of the linear fog/background color (0.015, 0.025, 0.04).
        constexpr float clear_color[4]{
            0.12835404f, 0.17184409f, 0.22091636f, 1.0f};
        command_list->ClearUnorderedAccessViewFloat(
            frame_buffer_uav_.get_gpu(),
            frame_buffer_uav_.get_cpu(),
            frame_buffer_.get(),
            clear_color,
            0,
            nullptr);

        command_list->SetComputeRootSignature(root_signature_.Get());
        command_list->SetPipelineState(pipeline_state_.Get());

        command_list->SetComputeRootConstantBufferView(
            static_cast<UINT>(RootParameter::CAMERA),
            frame.camera.get_address());
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParameter::INSTANCES),
            persistent.instance_transform->GetGPUVirtualAddress());
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParameter::VERTEX_DECODE_PARAMS),
            persistent.vertex_decode_params->GetGPUVirtualAddress());
        command_list->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParameter::GEOMETRY),
            geometry_views_.get_gpu());
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParameter::SUBMESHES),
            persistent.submesh->GetGPUVirtualAddress());
        command_list->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParameter::VISIBILITY_BUFFER),
            visibility_buffer);
        command_list->SetComputeRootShaderResourceView(
            static_cast<UINT>(RootParameter::MATERIALS),
            persistent.material->GetGPUVirtualAddress());
        command_list->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParameter::TEXTURES),
            persistent.texture_descriptors.get_gpu());
        command_list->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParameter::MATERIAL_SAMPLER),
            persistent.samplers.get_gpu(persistent.wrap_sampler));
        command_list->SetComputeRootDescriptorTable(
            static_cast<UINT>(RootParameter::FRAME_BUFFER),
            frame_buffer_uav_.get_gpu());

        command_list->Dispatch(
            dispatch_groups(width, THREAD_COUNT_X),
            dispatch_groups(height, THREAD_COUNT_Y),
            1);
    }

} // namespace fjr::render
