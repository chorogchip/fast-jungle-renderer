#include "FastJungle/renderer/RendererMain.hpp"

#include <iomanip>
#include <sstream>
#include <string_view>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/WindowsUtils.hpp"

namespace fjr::render {

    namespace {

        constexpr bool DRAW_TRIANGLE_IDS = false;

    } // namespace

    void RendererMain::log_impostor_probe(
        const data::DataPerFrame& frame) const {

        if (impostor_probe_targets_.empty()) {
            return;
        }

        const UINT64 byte_size = static_cast<UINT64>(std::max(
            data_persistant_.mesh_lod_count,
            1u)) * sizeof(uint32_t);
        D3D12_RANGE read_range{0, byte_size};
        void* mapped = nullptr;
        dx::abort_failed(frame.bin_counts_readback->Map(
            0, &read_range, &mapped));

        const auto* counts = static_cast<const uint32_t*>(mapped);

        uint64_t all_instances = 0;
        uint64_t visible_instances = 0;
        uint64_t final_lod_instances = 0;
        uint64_t selected_triangles = 0;

        std::ostringstream message;
        message << std::fixed << std::setprecision(2);

        for (const auto& target : impostor_probe_targets_) {
            uint64_t target_visible = 0;
            uint64_t target_triangles = 0;
            std::vector<uint32_t> lod_instances;
            lod_instances.reserve(target.mesh_lod_ids.size());

            for (std::size_t lod = 0;
                 lod < target.mesh_lod_ids.size();
                 ++lod) {

                const uint32_t instance_count =
                    counts[target.mesh_lod_ids[lod]];
                lod_instances.push_back(instance_count);
                target_visible += instance_count;
                target_triangles += static_cast<uint64_t>(instance_count) *
                    target.triangles_per_lod[lod];
            }

            const uint64_t target_final = lod_instances.back();
            const uint64_t target_final_triangles = target_final *
                target.triangles_per_lod.back();
            const double target_visible_percent =
                target.total_instance_count == 0 ? 0.0 :
                100.0 * static_cast<double>(target_visible) /
                    target.total_instance_count;

            message
                << "  " << target.name
                << " total=" << target.total_instance_count
                << " visible=" << target_visible
                << " (" << target_visible_percent << "%)"
                << " lod_instances=[";
            for (std::size_t lod = 0; lod < lod_instances.size(); ++lod) {
                if (lod != 0) message << ", ";
                message << "L" << lod << ':' << lod_instances[lod];
            }
            message
                << "] final_tri=" << target_final_triangles
                << " card_tri(2/4/8)=" << target_final * 2u
                << '/' << target_final * 4u
                << '/' << target_final * 8u
                << '\n';

            all_instances += target.total_instance_count;
            visible_instances += target_visible;
            final_lod_instances += target_final;
            selected_triangles += target_triangles;
        }

        const double visible_percent = all_instances == 0 ? 0.0 :
            100.0 * static_cast<double>(visible_instances) / all_instances;
        const double final_percent = all_instances == 0 ? 0.0 :
            100.0 * static_cast<double>(final_lod_instances) / all_instances;
        const uint64_t final_card_triangles_2 = final_lod_instances * 2u;
        const uint64_t final_card_triangles_4 = final_lod_instances * 4u;
        const uint64_t final_card_triangles_8 = final_lod_instances * 8u;

        const D3D12_RANGE no_cpu_writes{0, 0};
        frame.bin_counts_readback->Unmap(0, &no_cpu_writes);

        log::Logger::g_logger_debug_out
            << "[ImpostorProbe] RiverForest/QueenForest total="
            << all_instances
            << " visible=" << visible_instances
            << " (" << visible_percent << "%)"
            << " final_lod_candidates=" << final_lod_instances
            << " (" << final_percent << "%)"
            << " selected_tri=" << selected_triangles
            << " final_card_tri(2/4/8)="
            << final_card_triangles_2 << '/'
            << final_card_triangles_4 << '/'
            << final_card_triangles_8 << '\n'
            << message.str();
        log::Logger::g_logger_debug_out.flush_debug_string();
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

        std::vector<uint64_t> mesh_instance_counts(
            scene.meshes.size(), 0);
        for (const auto& batch : scene.point_batches) {
            mesh_instance_counts[batch.mesh] += batch.instances.count;
        }
        for (const auto& instance : scene.static_mesh_instances) {
            ++mesh_instance_counts[instance.mesh];
        }

        // This is deliberately a narrow probe, not an impostor policy.
        // These are the high-cost tree families for which an 8-direction card
        // would replace the final mesh LOD.
        for (std::size_t mesh_id = 0; mesh_id < scene.meshes.size(); ++mesh_id) {
            const auto& mesh = scene.meshes[mesh_id];
            const std::string_view name{
                scene.strings.data() + mesh.name};
            if (!name.starts_with("RiverForest_") &&
                !name.starts_with("QueenForest_")) {
                continue;
            }

            ImpostorProbeTarget target;
            target.name = name;
            target.total_instance_count = mesh_instance_counts[mesh_id];
            target.mesh_lod_ids.reserve(mesh.lod_count);
            target.triangles_per_lod.reserve(mesh.lod_count);

            for (uint32_t local_lod = 0;
                 local_lod < mesh.lod_count;
                 ++local_lod) {

                const uint32_t mesh_lod_id =
                    mesh.lod_offset + local_lod;
                const auto& lod = scene.mesh_lods[mesh_lod_id];
                uint32_t triangle_count = 0;
                for (uint32_t submesh = 0;
                     submesh < lod.submesh_count;
                     ++submesh) {
                    triangle_count += scene.submeshes[
                        lod.submesh_offset + submesh].index_count / 3u;
                }
                target.mesh_lod_ids.push_back(mesh_lod_id);
                target.triangles_per_lod.push_back(triangle_count);
            }

            impostor_probe_targets_.push_back(std::move(target));
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
        triangle_id_pass_.init(
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
            if (++impostor_probe_readback_count_ % 8u == 0u) {
                log_impostor_probe(data_per_frame_[frame]);
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

        if constexpr (DRAW_TRIANGLE_IDS) {
            triangle_id_pass_.record(
                context,
                data_persistant_,
                data_per_frame_[frame],
                data_per_frame_[frame].camera.get_address(),
                desc_rtv_.get_cpu(frame),
                desc_dsv_.get_cpu(),
                swap_chain_.get_width(),
                swap_chain_.get_height());
        }
        else {
            forward_pass_.record(
                context,
                data_persistant_,
                data_per_frame_[frame],
                data_per_frame_[frame].camera.get_address(),
                desc_rtv_.get_cpu(frame),
                desc_dsv_.get_cpu(),
                swap_chain_.get_width(),
                swap_chain_.get_height());
        }

        swap_chain_.get_current_buffer().transition(
            context.get(), D3D12_RESOURCE_STATE_PRESENT);

        // end

        context.close();
        command_queue_.execute(context.get());
        context.set_fence_value(command_queue_.signal());
        swap_chain_.present();
    }

} // namespace fjr::render
