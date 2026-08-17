#include "FastJungle/renderer/data/geometry/BuilderGeomMeshLod.hpp"

#include <cstdint>
#include <limits>

namespace fjr::render::data::geom {

    namespace {
        constexpr uint32_t SOFTWARE_RASTER_OPAQUE = 1u << 0;
        constexpr uint32_t SOFTWARE_RASTER_ALPHA = 1u << 1;
    }

    std::vector<DataPersistent::MeshLod> BuilderGeomMeshLod::build(
        const scene::StaticScene& scene) {

        std::vector<DataPersistent::MeshLod> mesh_lods(
            scene.mesh_lods.size());

        for (const auto& mesh : scene.meshes) {
            for (uint32_t lod = 0; lod < mesh.lod_count; ++lod) {
                const uint32_t source_id = mesh.lod_offset + lod;
                const auto& source = scene.mesh_lods[source_id];
                auto& destination = mesh_lods[source_id];

                destination.submesh_offset = source.submesh_offset;
                destination.submesh_count = source.submesh_count;
                destination.lod_error = source.max_deviation;
                destination.next_lod_error = lod + 1 < mesh.lod_count
                    ? scene.mesh_lods[source_id + 1].max_deviation
                    : std::numeric_limits<float>::infinity();
                destination.software_raster_mask = lod >= 4
                    ? SOFTWARE_RASTER_OPAQUE | SOFTWARE_RASTER_ALPHA
                    : 0;
            }
        }

        return mesh_lods;
    }

}
