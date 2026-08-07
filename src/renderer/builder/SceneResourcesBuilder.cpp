#include "FastJungle/renderer/builder/SceneResourcesBuilder.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/ResourceUploader.hpp"
#include "FastJungle/renderer/builder/SceneTextureResourcesBuilder.hpp"
#include "FastJungle/renderer/data/RenderTypesCommon.hpp"
#include "FastJungle/renderer/data/RenderTypesPointBatch.hpp"

namespace fjr::render {
    namespace {
        constexpr UINT64 MIN_STAGING_PAGE_SIZE =
            1ull * 1024ull * 1024ull;
        constexpr std::size_t UPLOAD_PAGE_COUNT = 2;

        void upload_geometry(
            data::SceneResources& output,
            dx::ResourceUploader& uploader,
            ID3D12Device* device,
            const scene::StaticScene& scene) {
            auto& geometry = output.geometry;
            if (!scene.vertices.empty()) {
                geometry.vertices.init(
                    device,
                    static_cast<UINT64>(scene.vertices.size()) *
                        sizeof(scene::StaticScene::Vertex),
                    D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_FLAG_NONE,
                    D3D12_RESOURCE_STATE_COMMON);
                uploader.upload_buffer(
                    geometry.vertices,
                    std::as_bytes(
                        std::span<const scene::StaticScene::Vertex>{
                            scene.vertices}),
                    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
                geometry.vertex_view.BufferLocation =
                    geometry.vertices->GetGPUVirtualAddress();
                geometry.vertex_view.SizeInBytes = static_cast<UINT>(
                    scene.vertices.size() * sizeof(scene::StaticScene::Vertex));
                geometry.vertex_view.StrideInBytes = sizeof(scene::StaticScene::Vertex);
            }

            if (!scene.indices.empty()) {
                geometry.indices.init(
                    device,
                    static_cast<UINT64>(scene.indices.size()) *
                        sizeof(std::uint32_t),
                    D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_FLAG_NONE,
                    D3D12_RESOURCE_STATE_COMMON);
                uploader.upload_buffer(
                    geometry.indices,
                    std::as_bytes(std::span<const std::uint32_t>{
                        scene.indices}),
                    D3D12_RESOURCE_STATE_INDEX_BUFFER);
                geometry.index_view.BufferLocation =
                    geometry.indices->GetGPUVirtualAddress();
                geometry.index_view.SizeInBytes =
                    static_cast<UINT>(scene.indices.size() * sizeof(std::uint32_t));
                geometry.index_view.Format = DXGI_FORMAT_R32_UINT;
            }
        }

        void upload_materials(
            data::SceneResources& output,
            dx::ResourceUploader& uploader,
            ID3D12Device* device,
            const data::SceneResourcesTemp& source) {
            if (!source.materials.empty()) {
                output.materials.materials.init(
                    device,
                    static_cast<UINT64>(source.materials.size()) *
                        sizeof(data::StbufMaterial),
                    D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_FLAG_NONE,
                    D3D12_RESOURCE_STATE_COMMON);
                uploader.upload_buffer(
                    output.materials.materials,
                    std::as_bytes(std::span<const data::StbufMaterial>{
                        source.materials}),
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }
            if (!source.texture_bindings.empty()) {
                output.materials.texture_bindings.init(
                    device,
                    static_cast<UINT64>(source.texture_bindings.size()) *
                        sizeof(data::StbufTextureBinding),
                    D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_FLAG_NONE,
                    D3D12_RESOURCE_STATE_COMMON);
                uploader.upload_buffer(
                    output.materials.texture_bindings,
                    std::as_bytes(
                        std::span<const data::StbufTextureBinding>{
                            source.texture_bindings}),
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }
        }

        void upload_instances(
            data::SceneResources& output,
            dx::ResourceUploader& uploader,
            ID3D12Device* device,
            const scene::StaticScene& scene,
            const data::SceneResourcesTemp& source,
            std::span<const std::uint32_t> point_instance_order) {
            if (!point_instance_order.empty()) {
                output.instances.point_instances.init(
                    device,
                    static_cast<UINT64>(point_instance_order.size()) *
                        sizeof(scene::StaticScene::PointInstance),
                    D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_FLAG_NONE,
                    D3D12_RESOURCE_STATE_COMMON);
                uploader.upload_buffer_gathered(
                    output.instances.point_instances,
                    std::as_bytes(
                        std::span<const scene::StaticScene::PointInstance>{
                            scene.point_instances}),
                    sizeof(scene::StaticScene::PointInstance),
                    point_instance_order,
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            }
            if (!source.matrix_instances.empty()) {
                output.instances.matrix_instances.init(
                    device,
                    static_cast<UINT64>(source.matrix_instances.size()) *
                        sizeof(data::StbufMatrixInstance),
                    D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_FLAG_NONE,
                    D3D12_RESOURCE_STATE_COMMON);
                uploader.upload_buffer(
                    output.instances.matrix_instances,
                    std::as_bytes(
                        std::span<const data::StbufMatrixInstance>{
                            source.matrix_instances}),
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            }
            output.instances.point_instance_count =
                static_cast<std::uint32_t>(point_instance_order.size());
            output.instances.matrix_instance_count =
                static_cast<std::uint32_t>(source.matrix_instances.size());
        }

        void upload_draw_resources(
            data::SceneResources& output,
            dx::ResourceUploader& uploader,
            ID3D12Device* device,
            const data::SceneResourcesTemp& source) {
            if (!source.draw_metadata.empty()) {
                output.draws.metadata.init(
                    device,
                    static_cast<UINT64>(source.draw_metadata.size()) *
                        sizeof(data::StbufDrawMetadata),
                    D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_FLAG_NONE,
                    D3D12_RESOURCE_STATE_COMMON);
                uploader.upload_buffer(
                    output.draws.metadata,
                    std::as_bytes(
                        std::span<const data::StbufDrawMetadata>{
                            source.draw_metadata}),
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }
            output.draws.metadata_count =
                static_cast<std::uint32_t>(source.draw_metadata.size());
        }

        void upload_point_resources(
            data::SceneResources& output,
            dx::ResourceUploader& uploader,
            ID3D12Device* device,
            const data::SceneResourcesTemp& source) {
            const auto& points = source.points;
            if (!points.clusters.empty()) {
                output.points.clusters.init(
                    device,
                    static_cast<UINT64>(points.clusters.size()) *
                        sizeof(data::StbufPointCluster),
                    D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_FLAG_NONE,
                    D3D12_RESOURCE_STATE_COMMON);
                uploader.upload_buffer(
                    output.points.clusters,
                    std::as_bytes(std::span<const data::StbufPointCluster>{
                        points.clusters}),
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            }
            if (!points.mesh_batches.empty()) {
                output.points.mesh_batches.init(
                    device,
                    static_cast<UINT64>(points.mesh_batches.size()) *
                        sizeof(data::StbufPointMeshBatch),
                    D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_FLAG_NONE,
                    D3D12_RESOURCE_STATE_COMMON);
                uploader.upload_buffer(
                    output.points.mesh_batches,
                    std::as_bytes(
                        std::span<const data::StbufPointMeshBatch>{
                            points.mesh_batches}),
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            }
            if (!points.definitions.empty()) {
                output.points.definitions.init(
                    device,
                    static_cast<UINT64>(points.definitions.size()) *
                        sizeof(data::StbufPointDef),
                    D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_FLAG_NONE,
                    D3D12_RESOURCE_STATE_COMMON);
                uploader.upload_buffer(
                    output.points.definitions,
                    std::as_bytes(std::span<const data::StbufPointDef>{
                        points.definitions}),
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            }
            if (!points.draw_templates.empty()) {
                output.points.draw_templates.init(
                    device,
                    static_cast<UINT64>(points.draw_templates.size()) *
                        sizeof(data::StbufPointDraw),
                    D3D12_HEAP_TYPE_DEFAULT,
                    D3D12_RESOURCE_FLAG_NONE,
                    D3D12_RESOURCE_STATE_COMMON);
                uploader.upload_buffer(
                    output.points.draw_templates,
                    std::as_bytes(
                        std::span<const data::StbufPointDraw>{
                            points.draw_templates}),
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            }
            output.points.cluster_count =
                static_cast<std::uint32_t>(points.clusters.size());
            output.points.mesh_batch_count =
                static_cast<std::uint32_t>(points.mesh_batches.size());
            output.points.definition_count =
                static_cast<std::uint32_t>(points.definitions.size());
            output.points.draw_template_count =
                static_cast<std::uint32_t>(points.draw_templates.size());
            output.points.bin_count = points.bin_count;
            output.points.indirect_layout = points.indirect_layout;
        }

    } // namespace

    data::SceneResources SceneResourcesBuilder::build(
        const Context& context,
        const scene::StaticScene& scene,
        const data::SceneResourcesTemp& source,
        std::span<const std::uint32_t> point_instance_order) {
        if (context.device == nullptr ||
            context.command_queue == nullptr ||
            context.heap_srv_cbv_uav == nullptr ||
            context.heap_sampler == nullptr) {
            log::Logger::g_logger << log::abrt(
                "SceneResourcesBuilder requires "
                "device, command queue, and descriptor heaps.");
        }
        const UINT64 staging_page_size = std::max(
            MIN_STAGING_PAGE_SIZE,
            SceneTextureResourcesBuilder::get_max_upload_size(
                context.device,
                scene));
        data::SceneResources result;
        dx::ResourceUploader uploader;
        uploader.init(
            context.device,
            *context.command_queue,
            static_cast<std::size_t>(staging_page_size),
            UPLOAD_PAGE_COUNT);
        upload_geometry(result, uploader, context.device, scene);
        upload_materials(result, uploader, context.device, source);
        upload_instances(
            result,
            uploader,
            context.device,
            scene,
            source,
            point_instance_order);
        upload_draw_resources(result, uploader, context.device, source);
        upload_point_resources(result, uploader, context.device, source);

        SceneTextureResourcesBuilder::build(
            result.materials,
            uploader,
            context.device,
            *context.heap_srv_cbv_uav,
            *context.heap_sampler,
            scene);
        uploader.flush();
        uploader.reset();
        return result;
    }

    data::SceneFrameResources SceneResourcesBuilder::build_frame(
        ID3D12Device* device,
        const data::SceneResources& scene) {
        data::SceneFrameResources result;
        const auto instance_count = scene.instances.point_instance_count;
        const auto bin_count = scene.points.bin_count;
        const auto command_count =
            scene.points.indirect_layout.total_command_capacity;

        if (instance_count != 0) {
            const auto byte_size =
                static_cast<UINT64>(instance_count) * sizeof(std::uint32_t);
            result.points.instance_bins.init(
                device,
                byte_size,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_COMMON);
            result.points.visible_instance_ids.init(
                device,
                byte_size,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_COMMON);
        }

        if (bin_count != 0) {
            const auto byte_size =
                static_cast<UINT64>(bin_count) * sizeof(std::uint32_t);
            result.points.bin_counts.init(
                device,
                byte_size,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_COMMON);
            result.points.bin_offsets.init(
                device,
                byte_size + sizeof(std::uint32_t),
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_COMMON);
            result.points.bin_cursors.init(
                device,
                byte_size,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_COMMON);
        }

        if (command_count != 0) {
            result.points.indirect_commands.init(
                device,
                static_cast<UINT64>(command_count) *
                    sizeof(data::IndirectDrawCommand),
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_COMMON);
        }
        return result;
    }
} // namespace fjr::render
