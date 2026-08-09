#include "FastJungle/renderer/RendererMain.hpp"

#include <string_view>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/WindowsUtils.hpp"

namespace fjr::render {

    uint64_t RendererMain::read_impostor_probe_candidate_count(
        const data::DataPerFrame& frame) const {

        if (impostor_probe_final_lod_ids_.empty()) {
            return 0;
        }

        const UINT64 byte_size = static_cast<UINT64>(std::max(
            data_persistant_.mesh_lod_count,
            1u)) * sizeof(uint32_t);
        D3D12_RANGE read_range{0, byte_size};
        void* mapped = nullptr;
        dx::abort_failed(frame.bin_counts_readback->Map(
            0, &read_range, &mapped));

        const auto* counts = static_cast<const uint32_t*>(mapped);
        uint64_t total = 0;
        for (const uint32_t mesh_lod_id : impostor_probe_final_lod_ids_) {
            total += counts[mesh_lod_id];
        }

        const D3D12_RANGE no_cpu_writes{0, 0};
        frame.bin_counts_readback->Unmap(0, &no_cpu_writes);
        return total;
    }

    void RendererMain::init(
        void* window,
        uint32_t width, uint32_t height,
        const scene::StaticScene& scene) {

        RendererBase::init(window, width, height, false);
        
        // init scene

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

        // This is deliberately a narrow probe, not an impostor policy.
        // These are the high-cost tree families for which an 8-direction card
        // would replace the final mesh LOD.
        for (const auto& mesh : scene.meshes) {
            const std::string_view name{
                scene.strings.data() + mesh.name};
            if (!name.starts_with("RiverForest_") &&
                !name.starts_with("QueenForest_")) {
                continue;
            }
            impostor_probe_final_lod_ids_.push_back(
                mesh.lod_offset + mesh.lod_count - 1u);
        }

        // init pass

        gpu_culling_pass_.init(
            device_.Get(),
            data_persistant_.submesh_count);
        forward_pass_.init(
            device_.Get(),
            data_persistant_.texture_descriptors.get_count(),
            data_persistant_.samplers.get_count(),
            data_persistant_.submesh_count);

    }

    void RendererMain::resize(uint32_t width, uint32_t height) {

        RendererBase::resize(width, height);
        camera.set_aspect_ratio(
            static_cast<float>(width) / static_cast<float>(height));
        // forward_pass_.views.desc_dsv = desc_dsv_.get_cpu();
        // forward_pass_.views.width = width;
        // forward_pass_.views.height = height;
    }

    void RendererMain::render() {

        // start

        const std::uint32_t frame =
            swap_chain_.get_current_frame();

        auto& context = command_contexts_[frame];
        command_queue_.wait(
            context.get_fence_value());

        if (impostor_probe_readback_ready_[frame]) {
            const uint64_t candidate_count =
                read_impostor_probe_candidate_count(data_per_frame_[frame]);
            if (++impostor_probe_readback_count_ % 8u == 0u) {
                log::Logger::g_logger_debug_out
                    << "[ImpostorProbe] final-LOD RiverForest/QueenForest "
                    << "visible instances (card candidates): "
                    << candidate_count << '\n';
                log::Logger::g_logger_debug_out.flush_debug_string();
            }
        }

        context.reset();

        // camera

        data_per_frame_[frame].camera.data().fill_from_camera(
            camera,
            swap_chain_.get_height(),
            data_persistant_.spatial_cluster_count,
            data_persistant_.mesh_lod_count);

        // prepare pass

        swap_chain_.get_current_buffer().transition(
            context.get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        context.SetDescriptorHeaps(
            heap_sampler_.get(),
            heap_srv_cbv_uav_.get());

        // record

        gpu_culling_pass_.record(
            context,
            data_persistant_,
            data_per_frame_[frame]);
        impostor_probe_readback_ready_[frame] = true;

        forward_pass_.record(
            context,
            data_persistant_,
            data_per_frame_[frame],
            data_per_frame_[frame].camera.get_address(),
            desc_rtv_.get_cpu(frame),
            desc_dsv_.get_cpu(),
            swap_chain_.get_width(),
            swap_chain_.get_height());

        swap_chain_.get_current_buffer().transition(
            context.get(), D3D12_RESOURCE_STATE_PRESENT);

        // end

        context.close();
        command_queue_.execute(context.get());
        context.set_fence_value(command_queue_.signal());
        swap_chain_.present();
    }

} // namespace fjr::render
