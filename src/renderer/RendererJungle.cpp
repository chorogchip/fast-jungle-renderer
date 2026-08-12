#include "FastJungle/renderer/RendererJungle.hpp"

#include <utility>

namespace fjr::render {

    void RendererJungle::init(
        void* window,
        uint32_t width, uint32_t height,
        const scene::StaticScene& scene) {

        RendererBase::init(window, width, height, false);

        dx::ResourceUploader uploader{};
        uploader.init(
            device_.Get(), command_queue_,
            128ull * 1024ull * 1024ull, 2);

        data_persistant_ = data::DataPersistent::build(
            scene, device_.Get(), uploader, heap_srv_cbv_uav_, heap_sampler_);

        uploader.wait();
        uploader.reset();

        // init camera

        DirectX::XMVECTOR scale;
        DirectX::XMVECTOR rotation_vector;
        DirectX::XMVECTOR translation;
        DirectX::XMMatrixDecompose(
            &scale,
            &rotation_vector,
            &translation,
            DirectX::XMLoadFloat4x4(&scene.camera.world_transform));

        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT4 rotation{};
        DirectX::XMStoreFloat3(&position, translation);
        DirectX::XMStoreFloat4(&rotation, rotation_vector);

        camera.init(
            position, rotation,
            2.0f * std::atan(
                0.5f * scene.camera.vertical_aperture / scene.camera.focal_length),
            static_cast<float>(std::max(width, 1u)) /
            static_cast<float>(std::max(height, 1u)),
            scene.camera.clipping_range.x, scene.camera.clipping_range.y,
            1.0f, 0.04f);

        for (auto& frame : data_per_frame_) {
            frame = data::DataPerFrame::build(
                device_.Get(),
                data_persistant_.instance_count,
                data_persistant_.submesh_count);
        }

        // init pass

        gpu_culling_pass_.init(
            device_.Get(),
            heap_srv_cbv_uav_,
            data_persistant_.mesh_lod_count,
            data_persistant_.spatial_cluster_count,
            data_persistant_.instance_count,
            data_persistant_.submesh_count);

        create_pass_views();
        create_pass_targets(width, height);

        PassVisibility::PassVisibilityResources visibility_resources{};
        visibility_resources.frames.resize(FRAME_COUNT);
        PassResolve::PassResolveResources resolve_resources{};
        resolve_resources.cameras.resize(FRAME_COUNT);
        for (uint32_t frame = 0; frame < FRAME_COUNT; ++frame) {
            visibility_resources.frames[frame] =
                PassVisibility::PassVisibilityFrameResources{
                .camera = data_per_frame_[frame].camera.get_address(),
                .inputs = visibility_input_views_[frame],
                .indirect_draws = data_per_frame_[frame].indirect_gpu_draw.get(),
                .indirect_draw_counts =
                    data_per_frame_[frame].indirect_gpu_draw_counts.get(),
            };
            resolve_resources.cameras[frame] =
                data_per_frame_[frame].camera.get_address();
        }

        visibility_resources.textures = data_persistant_.texture_descriptors;
        visibility_resources.samplers = pass_samplers_;
        visibility_resources.opaque_vertices = {
            .BufferLocation = data_persistant_.vertex_opaque_visibility
                ->GetGPUVirtualAddress(),
            .SizeInBytes = static_cast<UINT>(
                data_persistant_.vertex_opaque_visibility.get_byte_size()),
            .StrideInBytes = sizeof(data::DataPersistent::OpaqueVertex0),
        };
        visibility_resources.alpha_vertices = {
            .BufferLocation = data_persistant_.vertex_alpha_visibility
                ->GetGPUVirtualAddress(),
            .SizeInBytes = static_cast<UINT>(
                data_persistant_.vertex_alpha_visibility.get_byte_size()),
            .StrideInBytes = sizeof(data::DataPersistent::AlphaVertex0),
        };
        visibility_resources.indices = {
            .BufferLocation = data_persistant_.index->GetGPUVirtualAddress(),
            .SizeInBytes = static_cast<UINT>(
                data_persistant_.index.get_byte_size()),
            .Format = DXGI_FORMAT_R32_UINT,
        };
        visibility_resources.render_target = visibility_rtv_.get_cpu();
        visibility_resources.depth_stencil = desc_dsv_.get_cpu();
        visibility_resources.indirect_draw_capacity_per_class =
            data_persistant_.submesh_count;
        visibility_pass_.init(
            device_.Get(),
            std::move(visibility_resources));

        resolve_resources.inputs = resolve_views_;
        resolve_resources.textures = data_persistant_.texture_descriptors;
        resolve_resources.samplers = pass_samplers_;
        resolve_resources.frame_buffer_uav = frame_buffer_uav_;
        resolve_pass_.init(
            device_.Get(),
            std::move(resolve_resources));
    }

    void RendererJungle::create_pass_views() {

        for (auto& views : visibility_input_views_) {
            views = heap_srv_cbv_uav_.alloc(4);
        }
        resolve_views_ = heap_srv_cbv_uav_.alloc(10);
        visibility_uav_ = heap_srv_cbv_uav_.alloc();
        visibility_clear_uav_ = heap_cpu_srv_cbv_uav_.alloc();
        visibility_rtv_ = heap_rtv_.alloc();
        frame_buffer_uav_ = heap_srv_cbv_uav_.alloc();
        frame_buffer_clear_uav_ = heap_cpu_srv_cbv_uav_.alloc();
        pass_samplers_ = heap_sampler_.alloc(2);

        for (uint32_t frame = 0; frame < FRAME_COUNT; ++frame) {
            auto& views = visibility_input_views_[frame];
            data_per_frame_[frame].visible_instance
                .create_structured_srv<uint32_t>(
                device_.Get(),
                views.get_cpu(0));
            data_persistant_.instance_transform
                .create_structured_srv<data::DataPersistent::InstanceTransform>(
                device_.Get(),
                views.get_cpu(1));
            data_persistant_.vertex_decode_params
                .create_structured_srv<data::DataPersistent::VertexDecodeParams>(
                device_.Get(),
                views.get_cpu(2));
            data_persistant_.material
                .create_structured_srv<data::DataPersistent::Material>(
                device_.Get(),
                views.get_cpu(3));
        }

        data_persistant_.instance_transform
            .create_structured_srv<data::DataPersistent::InstanceTransform>(
            device_.Get(),
            resolve_views_.get_cpu(0));
        data_persistant_.vertex_decode_params
            .create_structured_srv<data::DataPersistent::VertexDecodeParams>(
            device_.Get(),
            resolve_views_.get_cpu(1));
        data_persistant_.vertex_opaque_visibility.create_typed_srv(
            device_.Get(),
            resolve_views_.get_cpu(2),
            DXGI_FORMAT_R16G16B16A16_UNORM,
            0,
            data_persistant_.vertex_opaque_visibility
                .get_element_count<data::DataPersistent::OpaqueVertex0>());
        data_persistant_.vertex_opaque_shading
            .create_structured_srv<data::DataPersistent::OpaqueVertex1>(
            device_.Get(),
            resolve_views_.get_cpu(3));
        data_persistant_.vertex_alpha_visibility
            .create_structured_srv<data::DataPersistent::AlphaVertex0>(
            device_.Get(),
            resolve_views_.get_cpu(4));
        data_persistant_.vertex_alpha_shading
            .create_structured_srv<data::DataPersistent::AlphaVertex1>(
            device_.Get(),
            resolve_views_.get_cpu(5));
        data_persistant_.index.create_typed_srv(
            device_.Get(),
            resolve_views_.get_cpu(6),
            DXGI_FORMAT_R32_UINT,
            0,
            data_persistant_.index.get_element_count<uint32_t>());
        data_persistant_.submesh
            .create_structured_srv<data::DataPersistent::SubMesh>(
            device_.Get(),
            resolve_views_.get_cpu(7));
        data_persistant_.material
            .create_structured_srv<data::DataPersistent::Material>(
            device_.Get(),
            resolve_views_.get_cpu(9));

        device_->CopyDescriptorsSimple(
            1,
            pass_samplers_.get_cpu(0),
            data_persistant_.samplers.get_cpu(data_persistant_.wrap_sampler),
            D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        device_->CopyDescriptorsSimple(
            1,
            pass_samplers_.get_cpu(1),
            data_persistant_.samplers.get_cpu(data_persistant_.clamp_sampler),
            D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }

    void RendererJungle::create_pass_targets(
        uint32_t width,
        uint32_t height) {

        visibility_buffer_.reset();

        D3D12_RESOURCE_DESC visibility_description{};
        visibility_description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        visibility_description.Width = width;
        visibility_description.Height = height;
        visibility_description.DepthOrArraySize = 1;
        visibility_description.MipLevels = 1;
        visibility_description.Format = DXGI_FORMAT_R32G32_UINT;
        visibility_description.SampleDesc.Count = 1;
        visibility_description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        visibility_description.Flags =
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET |
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        visibility_buffer_.init(
            device_.Get(),
            visibility_description,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        visibility_buffer_.create_rtv(
            device_.Get(),
            visibility_rtv_.get_cpu(),
            0, 0, 1,
            DXGI_FORMAT_R32G32_UINT);
        visibility_buffer_.create_srv(
            device_.Get(),
            resolve_views_.get_cpu(8),
            dx::TextureViewRange{
                .first_mip = 0,
                .mip_count = 1,
                .first_slice = 0,
                .slice_count = 1,
            },
            DXGI_FORMAT_R32G32_UINT);
        visibility_buffer_.create_uav(
            device_.Get(),
            visibility_uav_.get_cpu(),
            0, 0, 1,
            DXGI_FORMAT_R32G32_UINT);
        visibility_buffer_.create_uav(
            device_.Get(),
            visibility_clear_uav_.get_cpu(),
            0, 0, 1,
            DXGI_FORMAT_R32G32_UINT);

        frame_buffer_.reset();

        D3D12_RESOURCE_DESC frame_description{};
        frame_description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        frame_description.Width = width;
        frame_description.Height = height;
        frame_description.DepthOrArraySize = 1;
        frame_description.MipLevels = 1;
        frame_description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        frame_description.SampleDesc.Count = 1;
        frame_description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        frame_description.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        frame_buffer_.init(
            device_.Get(),
            frame_description,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        frame_buffer_.create_uav(
            device_.Get(),
            frame_buffer_uav_.get_cpu(),
            0, 0, 1,
            DXGI_FORMAT_R8G8B8A8_UNORM);
        frame_buffer_.create_uav(
            device_.Get(),
            frame_buffer_clear_uav_.get_cpu(),
            0, 0, 1,
            DXGI_FORMAT_R8G8B8A8_UNORM);
    }

    void RendererJungle::resize(uint32_t width, uint32_t height) {

        RendererBase::resize(width, height);
        create_pass_targets(width, height);
        camera.set_aspect_ratio(
            static_cast<float>(width) / static_cast<float>(height));
    }

    void RendererJungle::render() {

        const uint32_t frame =
            swap_chain_.get_current_frame();

        auto& context = command_contexts_[frame];
        command_queue_.wait(
            context.get_fence_value());

        context.reset();

        data_per_frame_[frame].camera.data().fill_from_camera(
            camera,
            swap_chain_.get_width(),
            swap_chain_.get_height(),
            data_persistant_.spatial_cluster_count,
            data_persistant_.mesh_lod_count);

        context.SetDescriptorHeaps(
            heap_sampler_.get(),
            heap_srv_cbv_uav_.get());

        gpu_culling_pass_.record(
            context,
            data_persistant_,
            data_per_frame_[frame]);

        context.transition(
            visibility_buffer_,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        constexpr std::array<UINT, 4> visibility_clear_value{
            0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
        context.get()->ClearUnorderedAccessViewUint(
            visibility_uav_.get_gpu(),
            visibility_clear_uav_.get_cpu(),
            visibility_buffer_.get(),
            visibility_clear_value.data(),
            0,
            nullptr);

        context.transition(
            visibility_buffer_,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        visibility_pass_.record(
            context,
            frame,
            swap_chain_.get_width(),
            swap_chain_.get_height());

        context.transition(
            visibility_buffer_,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        context.transition(
            frame_buffer_,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        // The resolve UAV stores display-encoded values. This is the sRGB
        // encoding of the linear fog/background color (0.015, 0.025, 0.04).
        constexpr float frame_clear_color[4]{
            0.12835404f, 0.17184409f, 0.22091636f, 1.0f};
        context.get()->ClearUnorderedAccessViewFloat(
            frame_buffer_uav_.get_gpu(),
            frame_buffer_clear_uav_.get_cpu(),
            frame_buffer_.get(),
            frame_clear_color,
            0,
            nullptr);

        resolve_pass_.record(
            context,
            frame,
            swap_chain_.get_width(),
            swap_chain_.get_height());

        context.transition(
            frame_buffer_,
            D3D12_RESOURCE_STATE_COPY_SOURCE);

        context.transition(
            swap_chain_.get_current_buffer(),
            D3D12_RESOURCE_STATE_COPY_DEST);

        context.get()->CopyResource(
            swap_chain_.get_current_buffer().get(),
            frame_buffer_.get());

        context.transition(
            swap_chain_.get_current_buffer(),
            D3D12_RESOURCE_STATE_PRESENT);

        context.close();
        command_queue_.execute(context.get());
        context.set_fence_value(command_queue_.signal());
        swap_chain_.present();
    }

} // namespace fjr::render
