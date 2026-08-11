#include "FastJungle/renderer/RendererMain.hpp"

namespace fjr::render {

    void RendererMain::init(
        void* window,
        uint32_t width, uint32_t height,
        const scene::StaticScene& scene) {

        RendererBase::init(window, width, height, false);

        // init scene
        environment_light_ = scene.environment_light;

        dx::ResourceUploader uploader{};
        uploader.init(
            device_.Get(), command_queue_,
            128ull * 1024ull * 1024ull, 2);

        data_persistant_ = data::DataPersistent::build(
            scene, device_.Get(), uploader, heap_srv_cbv_uav_, heap_sampler_);

        uploader.flush();
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
                data_persistant_.mesh_lod_count,
                data_persistant_.submesh_count);
        }

        // init pass

        gpu_culling_pass_.init(
            device_.Get(),
            data_persistant_.submesh_count);

        visibility_pass_.init(
            device_.Get(),
            heap_srv_cbv_uav_,
            heap_rtv_,
            data_persistant_.submesh_count,
            width,
            height);

        resolve_pass_.init(
            device_.Get(),
            heap_srv_cbv_uav_,
            data_persistant_,
            data_persistant_.texture_descriptors.get_count(),
            width,
            height);
    }

    void RendererMain::resize(uint32_t width, uint32_t height) {

        RendererBase::resize(width, height);
        visibility_pass_.resize(device_.Get(), width, height);
        resolve_pass_.resize( device_.Get(), width, height);
        camera.set_aspect_ratio(
            static_cast<float>(width) / static_cast<float>(height));
    }

    void RendererMain::render() {

        const std::uint32_t frame =
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
            data_persistant_.mesh_lod_count,
            environment_light_);

        context.SetDescriptorHeaps(
            heap_sampler_.get(),
            heap_srv_cbv_uav_.get());

        gpu_culling_pass_.record(
            context,
            data_persistant_,
            data_per_frame_[frame]);

        visibility_pass_.record(
            context,
            data_persistant_,
            data_per_frame_[frame],
            desc_dsv_.get_cpu(),
            swap_chain_.get_width(),
            swap_chain_.get_height());

        resolve_pass_.record(
            context,
            data_persistant_,
            data_per_frame_[frame],
            visibility_pass_.get_srv(),
            swap_chain_.get_width(),
            swap_chain_.get_height());

        resolve_pass_.get_frame_buffer().transition(
            context.get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE);

        swap_chain_.get_current_buffer().transition(
            context.get(),
            D3D12_RESOURCE_STATE_COPY_DEST);

        context.get()->CopyResource(
            swap_chain_.get_current_buffer().get(),
            resolve_pass_.get_frame_buffer().get());

        swap_chain_.get_current_buffer().transition(
            context.get(),
            D3D12_RESOURCE_STATE_PRESENT);

        context.close();
        command_queue_.execute(context.get());
        context.set_fence_value(command_queue_.signal());
        swap_chain_.present();
    }

} // namespace fjr::render