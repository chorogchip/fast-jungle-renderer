#include "FastJungle/renderer/data/geometry/BuilderGeomSubmesh.hpp"

#include "FastJungle/core/util/EnumUtils.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render::data::geom {

    namespace {

        void set_static_mesh_raster_class(
            std::span<DataPersistent::SubMesh> destination,
            const scene::StaticScene& scene,
            uint32_t instance_id,
            data::EnumRasterClass raster_class) {

            const auto& instance = scene.static_mesh_instances[instance_id];
            const auto& mesh = scene.meshes[instance.mesh];

            for (uint32_t lod = 0; lod < mesh.lod_count; ++lod) {

                const auto& meshlod = scene.mesh_lods[mesh.lod_offset + lod];

                for (uint32_t sm = 0; sm < meshlod.submesh_count; ++sm)
                    destination[meshlod.submesh_offset + sm].raster_class = raster_class;
            }
        }
    }

	std::vector<DataPersistent::SubMesh> BuilderGeomSubmesh::build(
		const scene::StaticScene& scene) {

		std::vector<DataPersistent::SubMesh> submeshes{};
        submeshes.resize(scene.submeshes.size());

        for (uint32_t index = 0; index < scene.submeshes.size(); ++index) {

            const auto& source = scene.submeshes[index];
            auto& destination = submeshes[index];

            const bool alpha_tested = enm::has(source.flags,
                scene::StaticScene::EnumSubmeshFlag::ALPHA_TESTED);
            const bool double_sided = enm::has(source.flags,
                scene::StaticScene::EnumSubmeshFlag::DOUBLE_SIDED);

            destination.raster_class = alpha_tested
                ? data::EnumRasterClass::ALPHA_TESTED
                : data::EnumRasterClass::OPAQUE_SINGLE_SIDED;
            destination.material_id = source.material;
            destination.index_offset = source.index_offset;
            destination.index_count = source.index_count;
            destination.base_vertex = static_cast<int32_t>(source.vertex_offset);
        }

        set_static_mesh_raster_class(
            submeshes,
            scene,
            scene.components.pyramid.instance,
            EnumRasterClass::PYRAMID);

        for (uint32_t inst = 0; inst < scene.components.terrain.extended.count; ++inst) {
            set_static_mesh_raster_class(
                submeshes,
                scene,
                scene.components.terrain.extended.offset + inst,
                EnumRasterClass::TERRAIN);
        }
        for (uint32_t inst = 0; inst < scene.components.terrain.cinematic.count; ++inst) {
            set_static_mesh_raster_class(
                submeshes,
                scene,
                scene.components.terrain.extended.offset + inst,
                EnumRasterClass::TERRAIN);
        }

        set_static_mesh_raster_class(
            submeshes,
            scene,
            scene.components.river.instance,
            EnumRasterClass::RIVER);

        set_static_mesh_raster_class(
            submeshes,
            scene,
            scene.components.creek.instance,
            EnumRasterClass::RIVER);

		return submeshes;
	}
}
