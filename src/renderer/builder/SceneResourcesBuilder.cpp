#include "FastJungle/renderer/builder/SceneResourcesBuilder.hpp"

#include <cstdint>
#include <span>
#include <stdexcept>

#include "FastJungle/dx12/ResourceUploader.hpp"
#include "FastJungle/renderer/builder/SceneTextureResourcesBuilder.hpp"
#include "FastJungle/renderer/data/RenderTypesCommon.hpp"
#include "FastJungle/renderer/data/RenderTypesPointBatch.hpp"

namespace fjr::render {

    namespace {

        template<typename T>
        void upload_if_not_empty(
            dx::Buffer& output,
            dx::ResourceUploader& uploader,
            std::span<const T> source,
            D3D12_RESOURCE_STATES final_state) {

            if (source.empty()) {
                return;
            }

            uploader.upload_buffer(
                output,
                source,
                final_state);
        }

        void upload_geometry(
            data::SceneResources& output,
            dx::ResourceUploader& uploader,
            const scene::StaticScene& scene) {

            upload_if_not_empty(
                output.geometry.vertices,
                uploader,
                std::span<
                const scene::StaticScene::Vertex>{
                scene.vertices},
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

            upload_if_not_empty(
                output.geometry.indices,
                uploader,
                std::span<const std::uint32_t>{
                scene.indices},
                D3D12_RESOURCE_STATE_INDEX_BUFFER);
        }

        void upload_materials(
            data::SceneResources& output,
            dx::ResourceUploader& uploader,
            const data::SceneResourcesTemp& source) {

            upload_if_not_empty(
                output.materials.materials,
                uploader,
                std::span<const data::StbufMaterial>{
                source.materials},
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            upload_if_not_empty(
                output.materials.texture_bindings,
                uploader,
                std::span<
                const data::StbufTextureBinding>{
                source.texture_bindings},
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        void upload_instances(
            data::SceneResources& output,
            dx::ResourceUploader& uploader,
            const scene::StaticScene& scene,
            const data::SceneResourcesTemp& source) {

            upload_if_not_empty(
                output.instances.point_instances,
                uploader,
                std::span<
                const scene::StaticScene::PointInstance>{
                scene.point_instances},
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

            upload_if_not_empty(
                output.instances.matrix_instances,
                uploader,
                std::span<
                const data::StbufMatrixInstance>{
                source.matrix_instances},
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

            upload_if_not_empty(
                output.instances.point_draw_constants,
                uploader,
                std::span<const data::CbufPointDraw>{
                source.point_constants},
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

            upload_if_not_empty(
                output.instances.matrix_draw_constants,
                uploader,
                std::span<const data::CbufMatrixDraw>{
                source.matrix_constants},
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

            output.instances.point_instance_count =
                static_cast<std::uint32_t>(
                    scene.point_instances.size());

            output.instances.matrix_instance_count =
                static_cast<std::uint32_t>(
                    source.matrix_instances.size());

            output.instances.point_constant_count =
                static_cast<std::uint32_t>(
                    source.point_constants.size());

            output.instances.matrix_constant_count =
                static_cast<std::uint32_t>(
                    source.matrix_constants.size());
        }

        void upload_point_resources(
            data::SceneResources& output,
            dx::ResourceUploader& uploader,
            const data::SceneResourcesTemp& source) {

            const auto& points =
                source.points;

            upload_if_not_empty(
                output.points.clusters,
                uploader,
                std::span<
                const data::StbufPointCluster>{
                points.clusters},
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

            upload_if_not_empty(
                output.points.batches,
                uploader,
                std::span<
                const data::StbufPointBatch>{
                points.batches},
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

            upload_if_not_empty(
                output.points.definitions,
                uploader,
                std::span<
                const data::StbufPointDef>{
                points.definitions},
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

            upload_if_not_empty(
                output.points.draw_templates,
                uploader,
                std::span<
                const data::StbufPointDraw>{
                points.draw_templates},
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

            output.points.cluster_count =
                static_cast<std::uint32_t>(
                    points.clusters.size());

            output.points.batch_count =
                static_cast<std::uint32_t>(
                    points.batches.size());

            output.points.definition_count =
                static_cast<std::uint32_t>(
                    points.definitions.size());

            output.points.draw_template_count =
                static_cast<std::uint32_t>(
                    points.draw_templates.size());

            output.points.bin_count =
                points.bin_count;

            output.points.indirect_layout =
                points.indirect_layout;
        }

        void create_geometry_views(
            data::SceneResources& output,
            const scene::StaticScene& scene) {

            if (output.geometry.vertices) {

                output.geometry.vertex_view.BufferLocation =
                    output.geometry.vertices
                    ->GetGPUVirtualAddress();

                output.geometry.vertex_view.SizeInBytes =
                    static_cast<UINT>(
                        scene.vertices.size() *
                        sizeof(
                            scene::StaticScene::Vertex));

                output.geometry.vertex_view.StrideInBytes =
                    sizeof(
                        scene::StaticScene::Vertex);
            }

            if (output.geometry.indices) {

                output.geometry.index_view.BufferLocation =
                    output.geometry.indices
                    ->GetGPUVirtualAddress();

                output.geometry.index_view.SizeInBytes =
                    static_cast<UINT>(
                        scene.indices.size() *
                        sizeof(std::uint32_t));

                output.geometry.index_view.Format =
                    DXGI_FORMAT_R32_UINT;
            }
        }

    } // namespace

    data::SceneResources
        SceneResourcesBuilder::build(
            const Context& context,
            const scene::StaticScene& scene,
            const data::SceneResourcesTemp& source) {

        if (context.device == nullptr ||
            context.command_queue == nullptr) {

            throw std::invalid_argument(
                "SceneResourcesBuilder requires "
                "device and command queue.");
        }

        data::SceneResources result;

        dx::ResourceUploader uploader{
            context.device,
            *context.command_queue
        };

        upload_geometry(
            result,
            uploader,
            scene);

        upload_materials(
            result,
            uploader,
            source);

        upload_instances(
            result,
            uploader,
            scene,
            source);

        upload_point_resources(
            result,
            uploader,
            source);

        SceneTextureResourcesBuilder::build(
            result.materials,
            uploader,
            context.device,
            scene);

        uploader.finish();

        create_geometry_views(
            result,
            scene);

        return result;
    }

} // namespace fjr::render