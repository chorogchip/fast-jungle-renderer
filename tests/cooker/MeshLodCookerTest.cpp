#include "FastJungle/cooker/MeshLodCooker.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <span>
#include <vector>

namespace {

    using StaticScene = fjr::scene::StaticScene;

    void require(bool condition, const char* message) {
        if (!condition) {
            std::cerr << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    std::uint32_t append_grid(
        StaticScene& scene,
        std::uint32_t resolution) {

        const auto vertex_offset =
            static_cast<std::uint32_t>(scene.vertices.size());
        const auto index_offset =
            static_cast<std::uint32_t>(scene.indices.size());
        for (std::uint32_t z = 0; z <= resolution; ++z) {
            for (std::uint32_t x = 0; x <= resolution; ++x) {
                scene.vertices.push_back({
                    {static_cast<float>(x), 0.0f, static_cast<float>(z)},
                    {0.0f, 1.0f, 0.0f},
                    {
                        static_cast<float>(x) / resolution,
                        static_cast<float>(z) / resolution,
                    },
                });
            }
        }

        const auto row = resolution + 1;
        for (std::uint32_t z = 0; z < resolution; ++z) {
            for (std::uint32_t x = 0; x < resolution; ++x) {
                const auto a = z * row + x;
                const auto b = a + 1;
                const auto c = a + row;
                const auto d = c + 1;
                scene.indices.insert(
                    scene.indices.end(),
                    {a, c, b, b, c, d});
            }
        }

        const auto submesh_offset =
            static_cast<std::uint32_t>(scene.submeshes.size());
        scene.submeshes.push_back({
            .vertex_offset = vertex_offset,
            .vertex_count = (resolution + 1) * (resolution + 1),
            .index_offset = index_offset,
            .index_count = resolution * resolution * 6,
        });
        const auto lod_offset =
            static_cast<std::uint32_t>(scene.mesh_lods.size());
        scene.mesh_lods.push_back({
            .submesh_offset = submesh_offset,
            .submesh_count = 1,
        });
        const auto mesh_index = static_cast<std::uint32_t>(scene.meshes.size());
        scene.meshes.push_back({
            .lod_offset = lod_offset,
            .lod_count = 1,
        });
        return mesh_index;
    }

    std::uint32_t append_triangle(StaticScene& scene) {
        const auto vertex_offset =
            static_cast<std::uint32_t>(scene.vertices.size());
        scene.vertices.insert(scene.vertices.end(), {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        });
        const auto index_offset =
            static_cast<std::uint32_t>(scene.indices.size());
        scene.indices.insert(scene.indices.end(), {0, 1, 2});

        const auto submesh_offset =
            static_cast<std::uint32_t>(scene.submeshes.size());
        scene.submeshes.push_back({
            .vertex_offset = vertex_offset,
            .vertex_count = 3,
            .index_offset = index_offset,
            .index_count = 3,
        });
        const auto lod_offset =
            static_cast<std::uint32_t>(scene.mesh_lods.size());
        scene.mesh_lods.push_back({
            .submesh_offset = submesh_offset,
            .submesh_count = 1,
        });
        const auto mesh_index = static_cast<std::uint32_t>(scene.meshes.size());
        scene.meshes.push_back({
            .lod_offset = lod_offset,
            .lod_count = 1,
        });
        return mesh_index;
    }

    const StaticScene::Submesh& submesh_at(
        const StaticScene& scene,
        std::uint32_t mesh_index,
        std::uint32_t lod_index) {

        const auto& mesh = scene.meshes[mesh_index];
        const auto& lod = scene.mesh_lods[mesh.lod_offset + lod_index];
        return scene.submeshes[lod.submesh_offset];
    }

} // namespace

int main() {
    StaticScene scene;
    const auto grid_mesh = append_grid(scene, 32);
    const auto tiny_mesh = append_triangle(scene);
    const auto lod0_indices = scene.indices;

    const auto stats = fjr::cooker::MeshLodCooker::cook(scene);

    require(scene.meshes[grid_mesh].lod_count == 4, "Grid does not have four LODs.");
    require(scene.meshes[tiny_mesh].lod_count == 4, "Tiny mesh does not have four LODs.");
    require(std::equal(
        lod0_indices.begin(), lod0_indices.end(), scene.indices.begin()),
        "LOD0 indices changed.");

    const auto grid_lod0 = submesh_at(scene, grid_mesh, 0);
    constexpr std::array<float, 4> RATIOS{1.0f, 0.40f, 0.15f, 0.04f};
    std::uint32_t previous_count = grid_lod0.index_count;
    float previous_error = 0.0f;
    for (std::uint32_t lod_index = 0; lod_index < 4; ++lod_index) {
        const auto& mesh = scene.meshes[grid_mesh];
        const auto& lod = scene.mesh_lods[mesh.lod_offset + lod_index];
        const auto& submesh = submesh_at(scene, grid_mesh, lod_index);
        require(lod.submesh_count == 1, "LOD submesh count changed.");
        require(submesh.vertex_offset == grid_lod0.vertex_offset,
            "LOD vertex offset changed.");
        require(submesh.vertex_count == grid_lod0.vertex_count,
            "LOD vertex count changed.");
        require(submesh.index_count <= previous_count,
            "LOD index count increased.");
		if (lod_index > 0) {
			const auto target =
				(static_cast<std::uint32_t>(
					grid_lod0.index_count * RATIOS[lod_index]) / 3) * 3;
			require(submesh.index_count <= target,
				"Simplifiable grid did not reach its requested LOD ratio.");
		}
        require(submesh.index_count % 3 == 0, "LOD contains partial triangles.");
        require(lod.max_deviation >= previous_error,
            "LOD error is not monotonic.");
        previous_count = submesh.index_count;
        previous_error = lod.max_deviation;
    }
    require(submesh_at(scene, grid_mesh, 1).index_count < grid_lod0.index_count,
        "Grid LOD1 was not simplified.");

    const auto tiny_lod0 = submesh_at(scene, tiny_mesh, 0);
    for (std::uint32_t lod_index = 1; lod_index < 4; ++lod_index) {
        const auto& tiny_lod = submesh_at(scene, tiny_mesh, lod_index);
        require(tiny_lod.index_offset == tiny_lod0.index_offset &&
            tiny_lod.index_count == tiny_lod0.index_count,
            "Tiny mesh should reuse LOD0.");
    }

    require(stats.simplified_submeshes > 0, "No submesh was simplified.");
    require(stats.generated_index_count > 0, "No LOD index data was generated.");

    return EXIT_SUCCESS;
}
