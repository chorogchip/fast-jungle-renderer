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

        data_persistent_ = data::DataPersistent::build(
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
                data_persistent_.instance_count,
                data_persistent_.submesh_count);
        }

        // init pass

        gpu_culling_pass_.init(
            device_.Get(),
            heap_srv_cbv_uav_,
            data_persistent_.mesh_lod_count,
            data_persistent_.spatial_cluster_count,
            data_persistent_.instance_count,
            data_persistent_.submesh_count);

        create_pass_views();
        create_pass_targets(width, height);

        PassVisibility::PassVisibilityResources visibility_resources{};
        visibility_resources.frames.resize(FRAME_COUNT);
        PassSWRaster::Resources software_resources{};
        software_resources.frames.resize(FRAME_COUNT);
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
            software_resources.frames[frame] =
                PassSWRaster::FrameResources{
                .camera = data_per_frame_[frame].camera.get_address(),
                .visible_instances = data_per_frame_[frame].visible_instance
                    ->GetGPUVirtualAddress(),
                .batches = data_per_frame_[frame].software_batches.get(),
                .batch_count =
                    data_per_frame_[frame].software_batch_count.get(),
            };
            resolve_resources.cameras[frame] =
                data_per_frame_[frame].camera.get_address();
        }

        visibility_resources.textures = data_persistent_.texture_descriptors;
        visibility_resources.samplers = pass_samplers_;
        visibility_resources.opaque_vertices = {
            .BufferLocation = data_persistent_.vertex_opaque_visibility
                ->GetGPUVirtualAddress(),
            .SizeInBytes = static_cast<UINT>(
                data_persistent_.vertex_opaque_visibility.get_byte_size()),
            .StrideInBytes = sizeof(data::DataPersistent::OpaqueVertex0),
        };
        visibility_resources.alpha_vertices = {
            .BufferLocation = data_persistent_.vertex_alpha_visibility
                ->GetGPUVirtualAddress(),
            .SizeInBytes = static_cast<UINT>(
                data_persistent_.vertex_alpha_visibility.get_byte_size()),
            .StrideInBytes = sizeof(data::DataPersistent::AlphaVertex0),
        };
        visibility_resources.indices = {
            .BufferLocation = data_persistent_.index->GetGPUVirtualAddress(),
            .SizeInBytes = static_cast<UINT>(
                data_persistent_.index.get_byte_size()),
            .Format = DXGI_FORMAT_R32_UINT,
        };
        visibility_resources.render_target = visibility_rtv_.get_cpu();
        visibility_resources.depth_stencil = desc_dsv_.get_cpu();
        visibility_resources.indirect_draw_capacity_per_class =
            data_persistent_.submesh_count;
        visibility_pass_.init(
            device_.Get(),
            std::move(visibility_resources));

        software_resources.instances =
            data_persistent_.instance_transform->GetGPUVirtualAddress();
        software_resources.vertex_decode_params =
            data_persistent_.vertex_decode_params->GetGPUVirtualAddress();
        software_resources.submeshes =
            data_persistent_.submesh->GetGPUVirtualAddress();
        software_resources.raster_clusters =
            data_persistent_.raster_cluster->GetGPUVirtualAddress();
        software_resources.raster_cluster_vertices =
            data_persistent_.raster_cluster_vertices->GetGPUVirtualAddress();
        software_resources.raster_cluster_triangles =
            data_persistent_.raster_cluster_triangles->GetGPUVirtualAddress();
        software_resources.opaque_vertices = data_persistent_
            .vertex_opaque_visibility->GetGPUVirtualAddress();
        software_resources.alpha_vertices = data_persistent_
            .vertex_alpha_visibility->GetGPUVirtualAddress();
        sw_raster_pass_.init(
            device_.Get(),
            heap_srv_cbv_uav_,
            heap_cpu_srv_cbv_uav_,
            std::move(software_resources),
            width,
            height);
        update_software_resolve_views();

        resolve_resources.inputs = resolve_views_;
        resolve_resources.textures = data_persistent_.texture_descriptors;
        resolve_resources.samplers = pass_samplers_;
        resolve_resources.frame_buffer_uav = frame_buffer_uav_;
        resolve_resources.software_inputs.assign(
            software_resolve_views_.begin(),
            software_resolve_views_.end());
        resolve_pass_.init(
            device_.Get(),
            std::move(resolve_resources));
    }

    void RendererJungle::create_pass_views() {

        for (auto& views : visibility_input_views_) {
            views = heap_srv_cbv_uav_.alloc(4);
        }
        for (auto& views : software_resolve_views_) {
            views = heap_srv_cbv_uav_.alloc(7);
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
            data_persistent_.instance_transform
                .create_structured_srv<data::DataPersistent::InstanceTransform>(
                device_.Get(),
                views.get_cpu(1));
            data_persistent_.vertex_decode_params
                .create_structured_srv<data::DataPersistent::VertexDecodeParams>(
                device_.Get(),
                views.get_cpu(2));
            data_persistent_.material
                .create_structured_srv<data::DataPersistent::Material>(
                device_.Get(),
                views.get_cpu(3));

            data_per_frame_[frame].software_batches
                .create_structured_srv<data::DataPerFrame::SoftwareBatch>(
                device_.Get(),
                software_resolve_views_[frame].get_cpu(2));
            data_per_frame_[frame].visible_instance
                .create_structured_srv<uint32_t>(
                device_.Get(),
                software_resolve_views_[frame].get_cpu(3));
        }

        data_persistent_.raster_cluster
            .create_structured_srv<scene::StaticScene::RasterCluster>(
            device_.Get(),
            software_resolve_views_[0].get_cpu(4));
        data_persistent_.raster_cluster_vertices
            .create_structured_srv<uint32_t>(
            device_.Get(),
            software_resolve_views_[0].get_cpu(5));
        data_persistent_.raster_cluster_triangles
            .create_structured_srv<uint32_t>(
            device_.Get(),
            software_resolve_views_[0].get_cpu(6));
        for (uint32_t frame = 1; frame < FRAME_COUNT; ++frame) {
            device_->CopyDescriptorsSimple(
                3,
                software_resolve_views_[frame].get_cpu(4),
                software_resolve_views_[0].get_cpu(4),
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        data_persistent_.instance_transform
            .create_structured_srv<data::DataPersistent::InstanceTransform>(
            device_.Get(),
            resolve_views_.get_cpu(0));
        data_persistent_.vertex_decode_params
            .create_structured_srv<data::DataPersistent::VertexDecodeParams>(
            device_.Get(),
            resolve_views_.get_cpu(1));
        data_persistent_.vertex_opaque_visibility.create_typed_srv(
            device_.Get(),
            resolve_views_.get_cpu(2),
            DXGI_FORMAT_R16G16B16A16_UNORM,
            0,
            data_persistent_.vertex_opaque_visibility
                .get_element_count<data::DataPersistent::OpaqueVertex0>());
        data_persistent_.vertex_opaque_shading
            .create_structured_srv<data::DataPersistent::OpaqueVertex1>(
            device_.Get(),
            resolve_views_.get_cpu(3));
        data_persistent_.vertex_alpha_visibility
            .create_structured_srv<data::DataPersistent::AlphaVertex0>(
            device_.Get(),
            resolve_views_.get_cpu(4));
        data_persistent_.vertex_alpha_shading
            .create_structured_srv<data::DataPersistent::AlphaVertex1>(
            device_.Get(),
            resolve_views_.get_cpu(5));
        data_persistent_.index.create_typed_srv(
            device_.Get(),
            resolve_views_.get_cpu(6),
            DXGI_FORMAT_R32_UINT,
            0,
            data_persistent_.index.get_element_count<uint32_t>());
        data_persistent_.submesh
            .create_structured_srv<data::DataPersistent::SubMesh>(
            device_.Get(),
            resolve_views_.get_cpu(7));
        data_persistent_.material
            .create_structured_srv<data::DataPersistent::Material>(
            device_.Get(),
            resolve_views_.get_cpu(9));

        device_->CopyDescriptorsSimple(
            1,
            pass_samplers_.get_cpu(0),
            data_persistent_.samplers.get_cpu(data_persistent_.wrap_sampler),
            D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        device_->CopyDescriptorsSimple(
            1,
            pass_samplers_.get_cpu(1),
            data_persistent_.samplers.get_cpu(data_persistent_.clamp_sampler),
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

    void RendererJungle::update_software_resolve_views() {
        for (uint32_t frame = 0; frame < FRAME_COUNT; ++frame) {
            auto& key = sw_raster_pass_.get_key(frame);
            key.create_raw_srv(
                device_.Get(),
                software_resolve_views_[frame].get_cpu(0),
                0,
                key.get_element_count<uint32_t>());
            buffer_depth_.create_srv(
                device_.Get(),
                software_resolve_views_[frame].get_cpu(1),
                dx::TextureViewRange{
                    .first_mip = 0,
                    .mip_count = 1,
                    .first_slice = 0,
                    .slice_count = 1,
                },
                DXGI_FORMAT_R32_FLOAT);
        }
    }

    void RendererJungle::resize(uint32_t width, uint32_t height) {

        RendererBase::resize(width, height);
        create_pass_targets(width, height);
        sw_raster_pass_.resize(device_.Get(), width, height);
        update_software_resolve_views();
        camera.set_aspect_ratio(
            static_cast<float>(width) / static_cast<float>(height));
    }

    void RendererJungle::render() {

        const uint32_t frame =
            swap_chain_.get_current_frame();

        auto& context = command_contexts_[frame];
        auto& cull_context = cull_contexts_[frame];
        auto& software_context = software_contexts_[frame];
        auto& resolve_context = resolve_contexts_[frame];

        command_queue_.wait(
            resolve_context.get_fence_value());

        data_per_frame_[frame].camera.data().fill_from_camera(
            camera,
            swap_chain_.get_width(),
            swap_chain_.get_height(),
            data_persistent_.spatial_cluster_count,
            data_persistent_.mesh_lod_count);

        compute_queue_.wait(cull_context.get_fence_value());
        cull_context.reset();
        cull_context.SetDescriptorHeaps(
            heap_sampler_.get(),
            heap_srv_cbv_uav_.get());

        gpu_culling_pass_.record(
            cull_context,
            data_persistent_,
            data_per_frame_[frame]);

        cull_context.close();
        compute_queue_.execute(cull_context.get());
        const UINT64 cull_fence = compute_queue_.signal();
        cull_context.set_fence_value(cull_fence);

        command_queue_.wait(context.get_fence_value());
        command_queue_.wait(compute_queue_, cull_fence);
        context.reset();
        context.SetDescriptorHeaps(
            heap_sampler_.get(),
            heap_srv_cbv_uav_.get());

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
        context.transition(
            buffer_depth_,
            D3D12_RESOURCE_STATE_DEPTH_WRITE);

        visibility_pass_.record(
            context,
            frame,
            swap_chain_.get_width(),
            swap_chain_.get_height());

        context.close();
        command_queue_.execute(context.get());
        context.set_fence_value(command_queue_.signal());

        compute_queue_.wait(software_context.get_fence_value());
        software_context.reset();
        software_context.SetDescriptorHeaps(
            heap_sampler_.get(),
            heap_srv_cbv_uav_.get());
        sw_raster_pass_.record(software_context, frame);
        software_context.close();
        compute_queue_.execute(software_context.get());
        const UINT64 software_fence = compute_queue_.signal();
        software_context.set_fence_value(software_fence);

        command_queue_.wait(compute_queue_, software_fence);
        resolve_context.reset();
        resolve_context.SetDescriptorHeaps(
            heap_sampler_.get(),
            heap_srv_cbv_uav_.get());

        resolve_context.transition(
            visibility_buffer_,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        resolve_context.transition(
            buffer_depth_,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        resolve_context.transition(
            sw_raster_pass_.get_key(frame),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        resolve_context.transition(
            data_per_frame_[frame].software_batches,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        resolve_context.transition(
            frame_buffer_,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        // The resolve UAV stores display-encoded values. This is the sRGB
        // encoding of the linear fog/background color (0.015, 0.025, 0.04).
        constexpr float frame_clear_color[4]{
            0.12835404f, 0.17184409f, 0.22091636f, 1.0f};
        resolve_context.get()->ClearUnorderedAccessViewFloat(
            frame_buffer_uav_.get_gpu(),
            frame_buffer_clear_uav_.get_cpu(),
            frame_buffer_.get(),
            frame_clear_color,
            0,
            nullptr);

        resolve_pass_.record(
            resolve_context,
            frame,
            swap_chain_.get_width(),
            swap_chain_.get_height());

        resolve_context.transition(
            frame_buffer_,
            D3D12_RESOURCE_STATE_COPY_SOURCE);

        resolve_context.transition(
            swap_chain_.get_current_buffer(),
            D3D12_RESOURCE_STATE_COPY_DEST);

        resolve_context.get()->CopyResource(
            swap_chain_.get_current_buffer().get(),
            frame_buffer_.get());

        resolve_context.transition(
            swap_chain_.get_current_buffer(),
            D3D12_RESOURCE_STATE_PRESENT);

        resolve_context.close();
        command_queue_.execute(resolve_context.get());
        resolve_context.set_fence_value(command_queue_.signal());
        swap_chain_.present();
    }

} // namespace fjr::render
