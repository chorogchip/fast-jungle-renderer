#include "FastJungle/cooker/MeshLodCooker.hpp"

#include <meshoptimizer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "FastJungle/core/util/Logger.hpp"

namespace fjr::cooker {
    namespace {

        using StaticScene = scene::StaticScene;

        [[noreturn]]
        void fail(const char* message) {
            log::Logger::g_logger << log::abrt(message);
        }

        [[nodiscard]]
        std::uint32_t checked_u32(std::size_t value, const char* subject) {
            if (value > std::numeric_limits<std::uint32_t>::max()) {
                log::Logger::g_logger
                    << subject << " exceeds uint32_t."
                    << log::abrt();
            }
            return static_cast<std::uint32_t>(value);
        }

        [[nodiscard]]
        bool contains_instance(
            StaticScene::IndexRange range,
            std::uint32_t mesh,
            const StaticScene& scene) {

            if (range.count == 0) {
                return false;
            }
            if (range.offset > scene.static_mesh_instances.size() ||
                range.count > scene.static_mesh_instances.size() - range.offset) {
                fail("Terrain component range is invalid during LOD cooking.");
            }
            for (std::uint32_t local = 0; local < range.count; ++local) {
                if (scene.static_mesh_instances[range.offset + local].mesh == mesh) {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]]
        bool is_terrain_mesh(std::uint32_t mesh, const StaticScene& scene) {
            return contains_instance(
                scene.components.terrain.extended, mesh, scene) ||
                contains_instance(
                    scene.components.terrain.cinematic, mesh, scene);
        }

        [[nodiscard]]
        bool is_pyramid_mesh(std::uint32_t mesh, const StaticScene& scene) {
            return scene.static_mesh_instances[scene.components.pyramid.instance].mesh == mesh;
        }

        void validate_settings(const MeshLodCookSettings& settings) {
            if (settings.triangle_ratios[0] != 1.0f ||
                settings.max_relative_errors[0] != 0.0f ||
                settings.minimum_reduction < 0.0f ||
                settings.minimum_reduction >= 1.0f) {
                fail("Invalid mesh LOD cook settings.");
            }
            for (std::size_t lod = 1;
                lod < MeshLodCookSettings::LOD_COUNT;
                ++lod) {
                if (!(settings.triangle_ratios[lod] > 0.0f) ||
                    !(settings.triangle_ratios[lod] <
                        settings.triangle_ratios[lod - 1]) ||
                    !(settings.max_relative_errors[lod] >=
                        settings.max_relative_errors[lod - 1])) {
                    fail("Mesh LOD ratios or errors are not monotonic.");
                }
            }
        }

        struct SubmeshState final {
            StaticScene::Submesh base;
            std::vector<unsigned int> indices;
            std::uint32_t current_index_offset = StaticScene::INVALID_INDEX;
            std::uint32_t current_index_count = 0;
            float accumulated_error = 0.0f;
            float scale = 0.0f;
        };

    } // namespace

    MeshLodCookStats MeshLodCooker::cook(
        StaticScene& scene,
        const MeshLodCookSettings& settings) {

        static_assert(sizeof(unsigned int) == sizeof(std::uint32_t));
        static_assert(offsetof(StaticScene::Vertex, position) == 0);
        static_assert(offsetof(StaticScene::Vertex, normal) == 12);
        static_assert(offsetof(StaticScene::Vertex, uv) == 24);

        validate_settings(settings);

        const auto source_submeshes = std::move(scene.submeshes);
        const auto source_lods = std::move(scene.mesh_lods);
        std::size_t lod0_index_end = 0;
        for (const auto& mesh : scene.meshes) {
            if (mesh.lod_count == 0 || mesh.lod_offset >= source_lods.size()) {
                fail("MeshLodCooker source mesh has no LOD0.");
            }
            const auto& lod0 = source_lods[mesh.lod_offset];
            if (lod0.submesh_offset > source_submeshes.size() ||
                lod0.submesh_count >
                source_submeshes.size() - lod0.submesh_offset) {
                fail("MeshLodCooker source LOD0 range is invalid.");
            }
            for (std::uint32_t local = 0; local < lod0.submesh_count; ++local) {
                const auto& submesh =
                    source_submeshes[lod0.submesh_offset + local];
                if (submesh.index_offset > scene.indices.size() ||
                    submesh.index_count >
                    scene.indices.size() - submesh.index_offset) {
                    fail("MeshLodCooker source LOD0 index range is invalid.");
                }
                lod0_index_end = std::max(
                    lod0_index_end,
                    static_cast<std::size_t>(submesh.index_offset) +
                    submesh.index_count);
            }
        }
        // A second cook of an already cooked scene discards the generated tail.
        // StaticSceneBuilder emits every LOD0 index before this tail.
        scene.indices.resize(lod0_index_end);
        scene.submeshes.clear();
        scene.mesh_lods.clear();
        scene.submeshes.reserve(
            source_submeshes.size() * MeshLodCookSettings::LOD_COUNT);
        scene.mesh_lods.reserve(
            scene.meshes.size() * MeshLodCookSettings::LOD_COUNT);

        MeshLodCookStats stats;
        constexpr std::array<float, 5> ATTRIBUTE_WEIGHTS{
            0.5f, 0.5f, 0.5f, 1.0f, 1.0f };

        for (std::uint32_t mesh_index = 0;
            mesh_index < scene.meshes.size();
            ++mesh_index) {
            auto& mesh = scene.meshes[mesh_index];
            const auto& source_lod = source_lods[mesh.lod_offset];
            if (source_lod.submesh_count == 0 ||
                source_lod.submesh_offset > source_submeshes.size() ||
                source_lod.submesh_count >
                source_submeshes.size() - source_lod.submesh_offset) {
                fail("MeshLodCooker source submesh range is invalid.");
            }

            std::vector<SubmeshState> states;
            states.reserve(source_lod.submesh_count);
            for (std::uint32_t local = 0;
                local < source_lod.submesh_count;
                ++local) {
                const auto& submesh =
                    source_submeshes[source_lod.submesh_offset + local];
                if (submesh.index_count == 0 ||
                    submesh.index_count % 3 != 0 ||
                    submesh.vertex_count == 0 ||
                    submesh.index_offset > scene.indices.size() ||
                    submesh.index_count >
                    scene.indices.size() - submesh.index_offset ||
                    submesh.vertex_offset > scene.vertices.size() ||
                    submesh.vertex_count >
                    scene.vertices.size() - submesh.vertex_offset) {
                    fail("MeshLodCooker source submesh is invalid.");
                }
                for (std::uint32_t local_index = 0;
                    local_index < submesh.index_count;
                    ++local_index) {
                    if (scene.indices[submesh.index_offset + local_index] >=
                        submesh.vertex_count) {
                        fail("MeshLodCooker source index is invalid.");
                    }
                }

                SubmeshState state;
                state.base = submesh;
                state.current_index_offset = submesh.index_offset;
                state.current_index_count = submesh.index_count;
                state.indices.assign(
                    scene.indices.begin() + submesh.index_offset,
                    scene.indices.begin() +
                    submesh.index_offset + submesh.index_count);
                meshopt_optimizeVertexCache(
                    state.indices.data(),
                    state.indices.data(),
                    state.indices.size(),
                    submesh.vertex_count);
                std::copy(
                    state.indices.begin(),
                    state.indices.end(),
                    scene.indices.begin() + submesh.index_offset);
                state.scale = meshopt_simplifyScale(
                    &scene.vertices[submesh.vertex_offset].position.x,
                    submesh.vertex_count,
                    sizeof(StaticScene::Vertex));
                states.push_back(std::move(state));
                stats.logical_index_counts[0] += submesh.index_count;
            }

            mesh.lod_offset = checked_u32(scene.mesh_lods.size(), "Mesh LOD offset");
            mesh.lod_count = MeshLodCookSettings::LOD_COUNT;

            StaticScene::MeshLod lod0;
            lod0.submesh_offset = checked_u32(
                scene.submeshes.size(), "LOD0 submesh offset");
            lod0.submesh_count = source_lod.submesh_count;
            for (const auto& state : states) {
                scene.submeshes.push_back(state.base);
            }
            scene.mesh_lods.push_back(lod0);

            const bool lock_border =
                is_terrain_mesh(mesh_index, scene) ||
                is_pyramid_mesh(mesh_index, scene);

            for (std::size_t lod_index = 1;
                lod_index < MeshLodCookSettings::LOD_COUNT;
                ++lod_index) {
                StaticScene::MeshLod lod;
                lod.submesh_offset = checked_u32(
                    scene.submeshes.size(), "LOD submesh offset");
                lod.submesh_count = source_lod.submesh_count;

                for (auto& state : states) {
                    const auto original_count =
                        static_cast<std::size_t>(state.base.index_count);
                    const auto target_count = std::max<std::size_t>(
                        3,
                        (static_cast<std::size_t>(std::floor(
                            original_count *
                            settings.triangle_ratios[lod_index])) / 3) * 3);
                    const auto current_count = state.indices.size();
                    const auto original_triangles = original_count / 3;
                    const float absolute_error_limit =
                        settings.max_relative_errors[lod_index] * state.scale;
                    const float remaining_error =
                        absolute_error_limit - state.accumulated_error;

                    bool accepted = false;
                    if (original_triangles >= settings.minimum_triangle_count &&
                        target_count < current_count &&
                        state.scale > 0.0f &&
                        remaining_error > 0.0f) {
                        std::vector<unsigned int> simplified(current_count);
                        float relative_result_error = 0.0f;
                        const unsigned int options =
                            meshopt_SimplifyPermissive |
                            (lock_border ? meshopt_SimplifyLockBorder : 0u);
                        const auto result_count =
                            meshopt_simplifyWithAttributes(
                                simplified.data(),
                                state.indices.data(),
                                current_count,
                                &scene.vertices[state.base.vertex_offset].position.x,
                                state.base.vertex_count,
                                sizeof(StaticScene::Vertex),
                                &scene.vertices[state.base.vertex_offset].normal.x,
                                sizeof(StaticScene::Vertex),
                                ATTRIBUTE_WEIGHTS.data(),
                                ATTRIBUTE_WEIGHTS.size(),
                                nullptr,
                                target_count,
                                remaining_error / state.scale,
                                options,
                                &relative_result_error);

                        bool used_sloppy_fallback = false;
                        auto selected_count = result_count;
                        // Card-like foliage and other disconnected topology can
                        // stop well above its requested ratio. Keep the
                        // attribute-aware result when it reaches the target;
                        // otherwise use the position-only fallback. Terrain is
                        // excluded because its locked borders must remain exact.
                        if (!lock_border && result_count > target_count) {
                            std::vector<unsigned int> fallback(current_count);
                            float fallback_error = 0.0f;
                            // meshopt_simplifySloppy accepts relative error in
                            // [0, 1]; retain the tighter per-level budget.
                            const auto fallback_count = meshopt_simplifySloppy(
                                fallback.data(),
                                state.indices.data(),
                                current_count,
                                &scene.vertices[state.base.vertex_offset].position.x,
                                state.base.vertex_count,
                                sizeof(StaticScene::Vertex),
                                target_count,
                                std::min(
                                    remaining_error / state.scale,
                                    0.1f),
                                &fallback_error);
                            if (fallback_count >= 3 &&
                                fallback_count % 3 == 0 &&
                                fallback_count < selected_count) {
                                fallback.resize(fallback_count);
                                simplified = std::move(fallback);
                                selected_count = fallback_count;
                                relative_result_error = fallback_error;
                                used_sloppy_fallback = true;
                            }
                        }

                        const auto maximum_accepted_count =
                            static_cast<std::size_t>(std::floor(
                                current_count *
                                (1.0f - settings.minimum_reduction)));
                        if (selected_count >= 3 && selected_count % 3 == 0 &&
                            selected_count <= maximum_accepted_count) {
                            simplified.resize(selected_count);
                            meshopt_optimizeVertexCache(
                                simplified.data(),
                                simplified.data(),
                                simplified.size(),
                                state.base.vertex_count);
                            state.indices = std::move(simplified);
                            state.current_index_offset = checked_u32(
                                scene.indices.size(), "LOD index offset");
                            state.current_index_count = checked_u32(
                                state.indices.size(), "LOD index count");
                            scene.indices.insert(
                                scene.indices.end(),
                                state.indices.begin(),
                                state.indices.end());
                            state.accumulated_error +=
                                relative_result_error * state.scale;
                            stats.generated_index_count += selected_count;
                            ++stats.simplified_submeshes;
                            stats.sloppy_fallback_submeshes +=
                                used_sloppy_fallback ? 1u : 0u;
                            accepted = true;
                        }
                    }

                    if (!accepted) {
                        ++stats.reused_submeshes;
                    }

                    auto cooked = state.base;
                    cooked.index_offset = state.current_index_offset;
                    cooked.index_count = state.current_index_count;
                    scene.submeshes.push_back(cooked);
                    stats.logical_index_counts[lod_index] += cooked.index_count;
                    lod.max_deviation = std::max(
                        lod.max_deviation,
                        state.accumulated_error);
                }
                scene.mesh_lods.push_back(lod);
            }
        }

        return stats;
    }

} // namespace fjr::cooker