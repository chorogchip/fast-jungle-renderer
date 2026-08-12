#include "FastJungle/renderer/data/geometry/BuilderGeomMesh.hpp"

#include "FastJungle/core/util/EnumUtils.hpp"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render::data::geom {


    namespace {

        [[nodiscard]]
        DataPersistent::Mesh build_mesh(
            const scene::StaticScene& scene,
            uint32_t mesh_id) {

            using namespace DirectX;

            const auto& source_mesh = scene.meshes[mesh_id];

            if (source_mesh.lod_count >
                Consts::CULL_MAX_CONVENTIONAL_LODS) {
                log::Logger::g_logger << log::abrt(
                    "Mesh LOD count exceeds the GPU culling bucket layout.");
            }

            const auto& lod0 = scene.mesh_lods[source_mesh.lod_offset];

            math::AABB bounds{};

            for (uint32_t sm = 0; sm < lod0.submesh_count; ++sm) {

                const auto& submesh = scene.submeshes[lod0.submesh_offset + sm];

                for (uint32_t vtx = 0; vtx < submesh.vertex_count; ++vtx)
                    bounds.merge(scene.vertices[submesh.vertex_offset + vtx].position);
            }

            DataPersistent::Mesh result;
            result.bounds_center = bounds.get_center();
            result.lod_offset = source_mesh.lod_offset;
            result.lod_count = source_mesh.lod_count;

            auto center = DirectX::XMLoadFloat3(&result.bounds_center);

            if (!bounds.is_valid())
                return result;

            float rad = 0.0f;

            for (uint32_t sm = 0; sm < lod0.submesh_count; ++sm) {

                const auto& submesh = scene.submeshes[lod0.submesh_offset + sm];

                for (uint32_t vtx = 0; vtx < submesh.vertex_count; ++vtx) {

                    const auto pos = XMLoadFloat3(&scene.vertices[
                        submesh.vertex_offset + vtx].position);

                    rad = (std::max)(rad,
                        XMVectorGetX(XMVector3LengthSq(
                            XMVectorSubtract(center, pos))));
                }
            }

            result.bounds_radius = std::sqrt(rad);
            return result;
        }

    } // namespace

	std::vector<DataPersistent::Mesh> BuilderGeomMesh::build(
		const scene::StaticScene& scene) {

        std::vector<DataPersistent::Mesh> meshes{};
        meshes.resize(scene.meshes.size());

        for (uint32_t mesh_id = 0; mesh_id < scene.meshes.size(); ++mesh_id)
            meshes[mesh_id] = build_mesh(scene, mesh_id);

        for (const auto& impostor : scene.impostors) {
            if (impostor.direction_count >
                Consts::CULL_MAX_IMPOSTOR_DIRECTIONS) {
                log::Logger::g_logger << log::abrt(
                    "Impostor direction count exceeds the GPU culling bucket layout.");
            }

            const auto first_card_lod = scene.meshes[
                impostor.card_mesh_offset].lod_offset;
            auto& destination = meshes[impostor.mesh];
            destination.impostor_card_lod_offset = first_card_lod;
            destination.impostor_direction_count = impostor.direction_count;
        }

        return meshes;
	}
}
