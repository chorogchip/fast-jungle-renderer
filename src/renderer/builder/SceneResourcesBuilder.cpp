#include "FastJungle/renderer/builder/SceneResourcesBuilder.hpp"

#include <cstdint>
#include <span>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/ResourceUploader.hpp"
#include "FastJungle/renderer/builder/SceneTextureResourcesBuilder.hpp"
#include "FastJungle/renderer/data/RenderTypesCommon.hpp"
#include "FastJungle/renderer/data/RenderTypesPointBatch.hpp"

namespace fjr::render {
    namespace {
        void upload_geometry(
            data::SceneResources& output,
            dx::ResourceUploader& uploader,
            const scene::StaticScene& scene) {
            auto& geometry = output.geometry;
            uploader.upload_buffer(
                geometry.vertices,
                std::span<const scene::StaticScene::Vertex>{scene.vertices},
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            if (geometry.vertices) {
                geometry.vertex_view.BufferLocation =
                    geometry.vertices->GetGPUVirtualAddress();
                geometry.vertex_view.SizeInBytes = static_cast<UINT>(
                    scene.vertices.size() * sizeof(scene::StaticScene::Vertex));
                geometry.vertex_view.StrideInBytes = sizeof(scene::StaticScene::Vertex);
            }

            uploader.upload_buffer(
                geometry.indices,
                std::span<const std::uint32_t>{
                scene.indices},
                D3D12_RESOURCE_STATE_INDEX_BUFFER);
            if (geometry.indices) {
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
            const data::SceneResourcesTemp& source) {
            uploader.upload_buffer(
                output.materials.materials,
                std::span<const data::StbufMaterial>{
                source.materials},
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            uploader.upload_buffer(
                output.materials.texture_bindings,
                std::span<const data::StbufTextureBinding>{
                source.texture_bindings},
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        void upload_instances(
            data::SceneResources& output,
            dx::ResourceUploader& uploader,
            const scene::StaticScene& scene,
            const data::SceneResourcesTemp& source,
            std::span<const std::uint32_t> point_instance_order) {
            uploader.upload_buffer_gathered(
                output.instances.point_instances,
                std::span<
                const scene::StaticScene::PointInstance>{
                scene.point_instances},
                point_instance_order,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            uploader.upload_buffer(
                output.instances.matrix_instances,
                std::span<const data::StbufMatrixInstance>{
                source.matrix_instances},
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            uploader.upload_buffer(
                output.instances.point_draw_constants,
                std::span<const data::CbufPointDraw>{
                source.point_constants},
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            uploader.upload_buffer(
                output.instances.matrix_draw_constants,
                std::span<const data::CbufMatrixDraw>{
                source.matrix_constants},
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            output.instances.point_instance_count =
                static_cast<std::uint32_t>(point_instance_order.size());
            output.instances.matrix_instance_count =
                static_cast<std::uint32_t>(source.matrix_instances.size());
            output.instances.point_constant_count =
                static_cast<std::uint32_t>(source.point_constants.size());
            output.instances.matrix_constant_count =
                static_cast<std::uint32_t>(source.matrix_constants.size());
        }

        void upload_point_resources(
            data::SceneResources& output,
            dx::ResourceUploader& uploader,
            const data::SceneResourcesTemp& source) {
            const auto& points = source.points;
            uploader.upload_buffer(
                output.points.clusters,
                std::span<const data::StbufPointCluster>{
                points.clusters},
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            uploader.upload_buffer(
                output.points.mesh_batches,
                std::span<const data::StbufPointMeshBatch>{
                points.mesh_batches},
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            uploader.upload_buffer(
                output.points.definitions,
                std::span<const data::StbufPointDef>{
                points.definitions},
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            uploader.upload_buffer(
                output.points.draw_templates,
                std::span<const data::StbufPointDraw>{
                points.draw_templates},
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
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
        data::SceneResources result;
        dx::ResourceUploader uploader{
            context.device,
            *context.command_queue,
            context.command_lists};
        upload_geometry(result, uploader, scene);
        upload_materials(result, uploader, source);
        upload_instances(result, uploader, scene, source, point_instance_order);
        upload_point_resources(result, uploader, source);

        SceneTextureResourcesBuilder::build(
            result.materials,
            uploader,
            context.device,
            *context.heap_srv_cbv_uav,
            *context.heap_sampler,
            scene);
        uploader.finish();
        return result;
    }
} // namespace fjr::render
