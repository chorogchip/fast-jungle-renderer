#include "FastJungle/renderer/data/geometry/BuilderGeometry.hpp"

#include <cstdint>

#include "FastJungle/renderer/data/geometry/BuilderGeomMesh.hpp"
#include "FastJungle/renderer/data/geometry/BuilderGeomMeshLod.hpp"
#include "FastJungle/renderer/data/geometry/BuilderGeomSubmesh.hpp"
#include "FastJungle/renderer/data/geometry/BuilderGeomVertex.hpp"

namespace fjr::render::data {

    BuilderGeometry::Result BuilderGeometry::build(
        data::DataPersistent& output,
        dx::ResourceUploader& uploader,
        ID3D12Device* device,
        const scene::StaticScene& scene) {

        auto vertices = geom::BuilderGeomVertex::build(scene);
        auto submeshes = geom::BuilderGeomSubmesh::build(
            scene,
            vertices.submesh_base_vertices);
        auto mesh_lods = geom::BuilderGeomMeshLod::build(scene);

        Result result{};
        result.meshes = geom::BuilderGeomMesh::build(scene);

        output.vertex_opaque_visibility.init(
            device,
            static_cast<UINT64>(
                vertices.opaque_visibility.size() *
                sizeof(DataPersistent::OpaqueVertex0)),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.vertex_opaque_visibility,
            vertices.opaque_visibility,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        output.vertex_opaque_shading.init(
            device,
            static_cast<UINT64>(
                vertices.opaque_shading.size() *
                sizeof(DataPersistent::OpaqueVertex1)),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.vertex_opaque_shading,
            vertices.opaque_shading,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        output.vertex_alpha_visibility.init(
            device,
            static_cast<UINT64>(
                vertices.alpha_visibility.size() *
                sizeof(DataPersistent::AlphaVertex0)),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.vertex_alpha_visibility,
            vertices.alpha_visibility,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        output.vertex_alpha_shading.init(
            device,
            static_cast<UINT64>(
                vertices.alpha_shading.size() *
                sizeof(DataPersistent::AlphaVertex1)),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.vertex_alpha_shading,
            vertices.alpha_shading,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        output.index.init(
            device,
            static_cast<UINT64>(
                scene.indices.size() * sizeof(uint32_t)),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.index,
            scene.indices,
            D3D12_RESOURCE_STATE_INDEX_BUFFER |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        output.raster_cluster.init(
            device,
            static_cast<UINT64>(
                scene.raster_clusters.size() *
                sizeof(scene::StaticScene::RasterCluster)),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.raster_cluster,
            scene.raster_clusters,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        output.raster_cluster_vertices.init(
            device,
            static_cast<UINT64>(
                scene.raster_cluster_vertices.size() * sizeof(uint32_t)),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.raster_cluster_vertices,
            scene.raster_cluster_vertices,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        output.raster_cluster_triangles.init(
            device,
            static_cast<UINT64>(
                scene.raster_cluster_triangles.size() * sizeof(uint32_t)),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.raster_cluster_triangles,
            scene.raster_cluster_triangles,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        output.vertex_decode_params.init(
            device,
            static_cast<UINT64>(
                vertices.decode_params.size() *
                sizeof(DataPersistent::VertexDecodeParams)),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.vertex_decode_params,
            vertices.decode_params,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        output.submesh.init(
            device,
            static_cast<UINT64>(
                submeshes.size() * sizeof(DataPersistent::SubMesh)),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.submesh,
            submeshes,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        output.mesh_lod.init(
            device,
            static_cast<UINT64>(
                mesh_lods.size() * sizeof(DataPersistent::MeshLod)),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.mesh_lod,
            mesh_lods,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        output.mesh.init(
            device,
            static_cast<UINT64>(
                result.meshes.size() * sizeof(DataPersistent::Mesh)),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.mesh,
            result.meshes,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        return result;
    }

} // namespace fjr::render::data
