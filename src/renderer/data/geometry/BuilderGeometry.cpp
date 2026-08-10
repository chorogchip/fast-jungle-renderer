#include "FastJungle/renderer/data/geometry/BuilderGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include "FastJungle/core/util/EnumUtils.hpp"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/core/math/AABB.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"
#include "FastJungle/renderer/data/geometry/BuilderGeomVertex.hpp"
#include "FastJungle/renderer/data/geometry/BuilderGeomSubmesh.hpp"
#include "FastJungle/renderer/data/geometry/BuilderGeomMesh.hpp"

namespace fjr::render::data {


    BuilderGeometry::Result BuilderGeometry::build(
        data::DataPersistent& output,
        dx::ResourceUploader& uploader,
        ID3D12Device* device,
        const scene::StaticScene& scene) {

        // vertices

        std::vector<DataPersistent::PackedPosition> positions;
        std::vector<DataPersistent::PackedNormal> normals;
        std::vector<DataPersistent::PackedUV> uvs;

        positions.reserve(scene.vertices.size());
        normals.reserve(scene.vertices.size());
        uvs.reserve(scene.vertices.size());

        auto vertex_decode_params = geom::BuilderGeomVertex::build(
            positions, normals, uvs, scene);

        output.vertex_pos.init(
            device,
            positions.size() * sizeof(positions[0]),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.vertex_pos,
            std::as_bytes(std::span<const DataPersistent::PackedPosition>{ positions }),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

        output.vertex_normal.init(
            device,
            normals.size() * sizeof(normals[0]),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.vertex_normal,
            std::as_bytes(std::span<const DataPersistent::PackedNormal>{ normals }),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

        output.vertex_uv.init(
            device,
            uvs.size() * sizeof(uvs[0]),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.vertex_uv,
            std::as_bytes(std::span<const DataPersistent::PackedUV>{ uvs }),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

        output.index.init(
            device,
            scene.indices.size() * sizeof(scene.indices[0]),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.index,
            std::as_bytes(std::span<const uint32_t>{ scene.indices }),
            D3D12_RESOURCE_STATE_INDEX_BUFFER);

        output.vertex_decode_params.init(
            device,
            vertex_decode_params.size() * sizeof(vertex_decode_params[0]),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.vertex_decode_params,
            std::as_bytes(std::span<const DataPersistent::VertexDecodeParams>{ vertex_decode_params }),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);


        std::vector<DataPersistent::MeshLod> mesh_lods;
        mesh_lods.resize(scene.mesh_lods.size());

        for (const auto& mesh : scene.meshes) {

            for (uint32_t lod = 0; lod < mesh.lod_count; ++lod) {

                const uint32_t source_id = mesh.lod_offset + lod;
                const auto& source = scene.mesh_lods[source_id];
                auto& destination = mesh_lods[source_id];

                destination.submesh_offset = source.submesh_offset;
                destination.submesh_count = source.submesh_count;
                destination.lod_error = source.max_deviation;

                destination.next_lod_error =
                    lod + 1 < mesh.lod_count ?
                    scene.mesh_lods[source_id + 1].max_deviation :
                    std::numeric_limits<float>::infinity();
            }
        }

        Result result{};
        result.meshes = geom::BuilderGeomMesh::build(scene);

        auto submeshes = geom::BuilderGeomSubmesh::build(scene);

        output.submesh.init(
            device,
            submeshes.size() * sizeof(submeshes[0]),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.submesh,
            std::as_bytes(std::span<const DataPersistent::SubMesh>{ submeshes }),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        output.mesh_lod.init(
            device,
            mesh_lods.size() * sizeof(mesh_lods[0]),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.mesh_lod,
            std::as_bytes(std::span<const DataPersistent::MeshLod>{ mesh_lods }),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        output.mesh.init(
            device,
            result.meshes.size() * sizeof(result.meshes[0]),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.mesh,
            std::as_bytes(std::span<const DataPersistent::Mesh>{ result.meshes }),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        return result;
    }

} // namespace fjr::render
