#include "FastJungle/renderer/builder/SceneResourcesBuilder.hpp"

#include "FastJungle/dx12/ResourceUploader.hpp"
#include "FastJungle/renderer/builder/SceneDrawBuilder.hpp"
#include "FastJungle/renderer/builder/SceneTextureResources.hpp"

#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>

namespace fjr::render {

    namespace {

        void create_buffer_resources(
            SceneResources& resources,
            dx::ResourceUploader& uploader,
            const scene::StaticScene& scene,
            const SceneRenderData& data) {

            uploader.upload_buffer(
                resources.buf_vertices,
                std::span<const scene::StaticScene::Vertex>{scene.vertices},
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            uploader.upload_buffer(
                resources.buf_indices,
                std::span<const std::uint32_t>{scene.indices},
                D3D12_RESOURCE_STATE_INDEX_BUFFER);
            uploader.upload_buffer(
                resources.buf_instances_point,
                std::span<const scene::StaticScene::PointInstance>{
                    scene.point_instances},
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            uploader.upload_buffer(
                resources.buf_instances_matrix,
                std::span<const SceneResources::MatrixInstance>{
                    data.matrix_instances},
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            uploader.upload_buffer(
                resources.buf_materials,
                std::span<const SceneResources::Material>{data.materials},
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            uploader.upload_buffer(
                resources.buf_texture_bindings,
                std::span<const SceneResources::TextureBinding>{
                    data.texture_bindings},
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            uploader.upload_buffer(
                resources.buf_cbuffer_point,
                std::span<const SceneResources::PointDrawConstants>{
                    data.point_draw_constants},
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            uploader.upload_buffer(
                resources.buf_cbuffer_matrix,
                std::span<const SceneResources::MatrixDrawConstants>{
                    data.matrix_draw_constants},
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        }

        void create_buffer_views(
            SceneResources& resources,
            const scene::StaticScene& scene) {

            if (resources.buf_cbuffer_matrix) {
                resources.view_cbuf_transform_matrix = dx::CBufferArrayView(
                    resources.buf_cbuffer_matrix->GetGPUVirtualAddress(),
                    sizeof(SceneResources::MatrixDrawConstants));
            }
            if (resources.buf_cbuffer_point) {
                resources.view_cbuf_transform_point = dx::CBufferArrayView(
                    resources.buf_cbuffer_point->GetGPUVirtualAddress(),
                    sizeof(SceneResources::PointDrawConstants));
            }
            if (resources.buf_vertices) {
                resources.view_vertices.BufferLocation =
                    resources.buf_vertices->GetGPUVirtualAddress();
                resources.view_vertices.SizeInBytes = static_cast<UINT>(
                    scene.vertices.size() *
                    sizeof(scene::StaticScene::Vertex));
                resources.view_vertices.StrideInBytes =
                    sizeof(scene::StaticScene::Vertex);
            }
            if (resources.buf_indices) {
                resources.view_indices.BufferLocation =
                    resources.buf_indices->GetGPUVirtualAddress();
                resources.view_indices.SizeInBytes = static_cast<UINT>(
                    scene.indices.size() * sizeof(std::uint32_t));
                resources.view_indices.Format = DXGI_FORMAT_R32_UINT;
            }
        }

    } // namespace

    SceneResourcesBuilder::BuildResult SceneResourcesBuilder::build(
        BuildContexts& context,
        const scene::StaticScene& scene,
        const RendererOptions& options) {

        if (context.device == nullptr || context.command_queue == nullptr) {
            throw std::invalid_argument(
                "SceneResourcesBuilder requires a device and command queue.");
        }

        auto resources = std::make_unique<SceneResources>();
        auto data = SceneDrawBuilder::build(scene, options);
        dx::ResourceUploader uploader{
            context.device,
            *context.command_queue};

        create_buffer_resources(*resources, uploader, scene, data);
        create_scene_texture_resources(
            *resources,
            uploader,
            context.device,
            scene);
        uploader.finish();
        create_buffer_views(*resources, scene);

        BuildResult result;
        result.resources = std::move(resources);
        result.draw_items = std::move(data.draw_items);
        return result;
    }

} // namespace fjr::render
