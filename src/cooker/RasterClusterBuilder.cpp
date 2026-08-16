#include "RasterClusterBuilder.hpp"

#include <meshoptimizer.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "FastJungle/core/util/CheckedCast.hpp"

namespace fjr::cooker {
    namespace {

        constexpr std::size_t CLUSTER_VERTEX_COUNT = 192;
        constexpr std::size_t CLUSTER_TRIANGLE_COUNT = 128;

        [[nodiscard]]
        uint32_t pack_triangle(const unsigned char* triangle) noexcept {
            return uint32_t{triangle[0]} |
                (uint32_t{triangle[1]} << 8u) |
                (uint32_t{triangle[2]} << 16u);
        }

    } // namespace

    void RasterClusterBuilder::build(scene::StaticScene& scene) {
        using util::checked_u32;

        for (auto& submesh : scene.submeshes) {
            const auto* indices = scene.indices.data() + submesh.index_offset;
            const auto* positions =
                &scene.vertices[submesh.vertex_offset].position.x;
            const std::size_t bound = meshopt_buildMeshletsBound(
                submesh.index_count,
                CLUSTER_VERTEX_COUNT,
                CLUSTER_TRIANGLE_COUNT);

            std::vector<meshopt_Meshlet> meshlets(bound);
            std::vector<unsigned int> vertices(submesh.index_count);
            std::vector<unsigned char> triangles(submesh.index_count);
            const std::size_t meshlet_count = meshopt_buildMeshlets(
                meshlets.data(),
                vertices.data(),
                triangles.data(),
                indices,
                submesh.index_count,
                positions,
                submesh.vertex_count,
                sizeof(scene::StaticScene::Vertex),
                CLUSTER_VERTEX_COUNT,
                CLUSTER_TRIANGLE_COUNT,
                0.0f);

            submesh.raster_cluster_offset = checked_u32(
                scene.raster_clusters.size(),
                "Raster cluster offset");
            submesh.raster_cluster_count = checked_u32(
                meshlet_count,
                "Raster cluster count");

            for (std::size_t meshlet_id = 0;
                meshlet_id < meshlet_count;
                ++meshlet_id) {
                const auto& meshlet = meshlets[meshlet_id];

                scene::StaticScene::RasterCluster cluster;
                cluster.vertex_offset = checked_u32(
                    scene.raster_cluster_vertices.size(),
                    "Raster cluster vertex offset");
                cluster.triangle_offset = checked_u32(
                    scene.raster_cluster_triangles.size(),
                    "Raster cluster triangle offset");
                cluster.vertex_count = meshlet.vertex_count;
                cluster.triangle_count = meshlet.triangle_count;
                scene.raster_clusters.push_back(cluster);

                scene.raster_cluster_vertices.insert(
                    scene.raster_cluster_vertices.end(),
                    vertices.begin() + meshlet.vertex_offset,
                    vertices.begin() +
                        meshlet.vertex_offset + meshlet.vertex_count);

                const auto* meshlet_triangles =
                    triangles.data() + meshlet.triangle_offset;
                for (uint32_t triangle = 0;
                    triangle < meshlet.triangle_count;
                    ++triangle) {
                    scene.raster_cluster_triangles.push_back(
                        pack_triangle(meshlet_triangles + triangle * 3u));
                }
            }
        }
    }

} // namespace fjr::cooker
