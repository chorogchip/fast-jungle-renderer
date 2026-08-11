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

        auto submeshes = geom::BuilderGeomSubmesh::build(scene);

        std::vector<DataPersistent::OpaqueVertex0> opaque0;
        std::vector<DataPersistent::OpaqueVertex1> opaque1;

        std::vector<DataPersistent::AlphaVertex0> alpha0;
        std::vector<DataPersistent::AlphaVertex1> alpha1;

        std::vector<DataPersistent::VertexDecodeParams> vertex_decode_params;
        vertex_decode_params.resize(scene.submeshes.size());

        opaque0.reserve(scene.vertices.size());
        opaque1.reserve(scene.vertices.size());
        alpha0.reserve(scene.vertices.size());
        alpha1.reserve(scene.vertices.size());

        for (uint32_t sm = 0; sm < static_cast<uint32_t>(scene.submeshes.size()); ++sm) {

            const auto& src_sm = scene.submeshes[sm];
            auto& dst_sm = submeshes[sm];
            auto& decode = vertex_decode_params[sm];

            const bool alpha = enm::has(src_sm.flags,
                scene::StaticScene::EnumSubmeshFlag::ALPHA_TESTED);

            math::AABB aabb_pos{};
            math::AABB aabb_uv{};

            for (uint32_t v = 0; v < src_sm.vertex_count; ++v) {
                const auto& src = scene.vertices[src_sm.vertex_offset + v];
                aabb_pos.merge(src.position);
                aabb_uv.merge(src.uv.x, src.uv.y, 0.0f);
            }

            const DirectX::XMFLOAT3 pos_min = aabb_pos.min;
            const DirectX::XMFLOAT3 pos_extent = aabb_pos.get_size();
            const DirectX::XMFLOAT3 uv_min = aabb_uv.min;
            const DirectX::XMFLOAT3 uv_extent = aabb_uv.get_size();

            decode.position_min = {
                pos_min.x, pos_min.y, pos_min.z, 0.0f };

            decode.position_extent = {
                pos_extent.x, pos_extent.y, pos_extent.z, 0.0f };

            decode.uv_min_extent = {
                uv_min.x, uv_min.y, uv_extent.x, uv_extent.y };

            if (alpha) {

                dst_sm.base_vertex = static_cast<int32_t>(alpha0.size());
                for (uint32_t v = 0; v < src_sm.vertex_count; ++v) {

                    const auto& src = scene.vertices[src_sm.vertex_offset + v];

                    const auto position = geom::BuilderGeomVertex::pack_position(
                        src.position, pos_min, pos_extent);

                    const auto uv = geom::BuilderGeomVertex::pack_uv(
                        src.uv, uv_min, uv_extent);

                    const auto normal = geom::BuilderGeomVertex::pack_normal(
                        src.normal);

                    alpha0.push_back({ position, uv });
                    alpha1.push_back({ normal });
                }

            } else {

                dst_sm.base_vertex = static_cast<int32_t>(opaque0.size());
                for (uint32_t v = 0; v < src_sm.vertex_count; ++v) {

                    const auto& src = scene.vertices[src_sm.vertex_offset + v];
                    const auto position = geom::BuilderGeomVertex::pack_position(
                        src.position, pos_min, pos_extent);

                    const auto uv = geom::BuilderGeomVertex::pack_uv(
                        src.uv, uv_min, uv_extent);

                    const auto normal = geom::BuilderGeomVertex::pack_normal(
                        src.normal);

                    opaque0.push_back({ position });
                    opaque1.push_back({ normal, uv });
                }
            }
        }

        output.vertex_opaque_visibility.init(
            device,
            opaque0.size() * sizeof(DataPersistent::OpaqueVertex0),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.vertex_opaque_visibility,
            std::as_bytes(std::span<const DataPersistent::OpaqueVertex0>{ opaque0 }),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        output.vertex_opaque_shading.init(
            device,
            opaque1.size() * sizeof(DataPersistent::OpaqueVertex1),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.vertex_opaque_shading,
            std::as_bytes(std::span<const DataPersistent::OpaqueVertex1>{ opaque1 }),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        output.vertex_alpha_visibility.init(
            device,
            alpha0.size() * sizeof(DataPersistent::AlphaVertex0),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.vertex_alpha_visibility,
            std::as_bytes(std::span<const DataPersistent::AlphaVertex0>{ alpha0}),
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        output.vertex_alpha_shading.init(
            device,
            alpha1.size() * sizeof(DataPersistent::AlphaVertex1),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.vertex_alpha_shading,
            std::as_bytes(std::span<const DataPersistent::AlphaVertex1>{alpha1 }),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        output.index.init(
            device,
            scene.indices.size() * sizeof(scene.indices[0]),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_COMMON);

        uploader.upload_buffer(
            output.index,
            std::as_bytes(std::span<const uint32_t>{ scene.indices }),
            D3D12_RESOURCE_STATE_INDEX_BUFFER |
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        output.vertex_decode_params.init(
            device,
            vertex_decode_params.size() *
            sizeof(DataPersistent::VertexDecodeParams),
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

} // namespace fjr::render::data