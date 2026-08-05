#include "FastJungle/renderer/builder/SceneDrawBuilder.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace fjr::render {

    namespace {

        [[nodiscard]]
        bool has_submesh_flag(
            scene::StaticScene::EnumSubmeshFlag flags,
            scene::StaticScene::EnumSubmeshFlag test) noexcept {

            return (
                static_cast<std::uint32_t>(flags) &
                static_cast<std::uint32_t>(test)) != 0;
        }

        [[nodiscard]]
        data::EnumPSOClass select_pso_class(
            scene::StaticScene::EnumSubmeshFlag flags) noexcept {

            return has_submesh_flag(
                flags,
                scene::StaticScene::EnumSubmeshFlag::
                DOUBLE_SIDED)
                ? data::EnumPSOClass::DOUBLE_SIDED
                : data::EnumPSOClass::SINGLE_SIDED;
        }

        [[nodiscard]]
        data::EnumDrawCpuFlag select_draw_flags(
            scene::StaticScene::EnumSubmeshFlag flags) noexcept {

            std::uint32_t value =
                static_cast<std::uint32_t>(
                    data::EnumDrawCpuFlag::DEFAULT);

            if (has_submesh_flag(
                flags,
                scene::StaticScene::EnumSubmeshFlag::
                DOUBLE_SIDED)) {

                value |= static_cast<std::uint32_t>(
                    data::EnumDrawCpuFlag::
                    DOUBLE_SIDED);
            }

            if (has_submesh_flag(
                flags,
                scene::StaticScene::EnumSubmeshFlag::
                ALPHA_BLENDED)) {

                value |= static_cast<std::uint32_t>(
                    data::EnumDrawCpuFlag::
                    ALPHA_BLENDED);
            }

            return static_cast<
                data::EnumDrawCpuFlag>(value);
        }

        void append_mesh_draws(
            std::vector<data::DrawFinalGPUIndirect>& output,
            const scene::StaticScene& scene,
            std::uint32_t mesh_index,
            data::EnumPointOrMatrix instance_class,
            std::uint32_t instance_offset,
            std::uint32_t instance_count,
            std::uint32_t transform_constant_index,
            const math::AABB& world_bounds,
            float world_scale) {

            if (instance_count == 0) {
                return;
            }

            const auto& mesh =
                scene.meshes[mesh_index];

            for (std::uint32_t local_lod = 0;
                local_lod < mesh.lod_count;
                ++local_lod) {

                const auto lod_index =
                    static_cast<std::size_t>(
                        mesh.lod_offset) +
                    local_lod;

                const auto& lod =
                    scene.mesh_lods[lod_index];

                const float next_lod_error =
                    local_lod + 1 < mesh.lod_count
                    ? scene.mesh_lods[
                        lod_index + 1].max_deviation
                    : std::numeric_limits<
                            float>::infinity();

                        for (std::uint32_t local_submesh = 0;
                            local_submesh <
                            lod.submesh_count;
                            ++local_submesh) {

                            const auto submesh_index =
                                static_cast<std::size_t>(
                                    lod.submesh_offset) +
                                local_submesh;

                            const auto& submesh =
                                scene.submeshes[submesh_index];

                            if (submesh.index_count == 0) {
                                continue;
                            }

                            data::DrawFinalGPUIndirect draw;

                            draw.constants.offset_instance =
                                instance_offset;

                            draw.constants.offset_material =
                                submesh.material ==
                                scene::StaticScene::INVALID_INDEX
                                ? static_cast<std::uint32_t>(
                                    scene.materials.size())
                                : submesh.material;

                            draw.instnace_class =
                                instance_class;

                            draw.pso_class =
                                select_pso_class(
                                    submesh.flags);

                            draw.flags =
                                select_draw_flags(
                                    submesh.flags);

                            draw.offset_cbuf_transform =
                                transform_constant_index;

                            draw.offset_index =
                                submesh.index_offset;

                            draw.offset_vertex =
                                submesh.vertex_offset;

                            draw.count_index =
                                submesh.index_count;

                            draw.count_instance =
                                instance_count;

                            draw.lod_index =
                                local_lod;

                            draw.world_bounds =
                                world_bounds;

                            draw.world_scale =
                                world_scale;

                            draw.lod_error =
                                lod.max_deviation;

                            draw.next_lod_error =
                                next_lod_error;

                            output.push_back(draw);
                        }
            }
        }

        void append_point_range(
            std::vector<data::DrawFinalGPUIndirect>& output,
            const scene::StaticScene& scene,
            const data::SceneBounds& bounds,
            scene::StaticScene::IndexRange range) {

            for (std::uint32_t local_batch = 0;
                local_batch < range.count;
                ++local_batch) {

                const auto batch_index =
                    range.offset + local_batch;

                const auto& batch =
                    scene.point_batches[batch_index];

                if (batch.instance_count == 0) {
                    continue;
                }

                const auto& definition =
                    scene.instanced_mesh_definitions[
                        batch.definition];

                // Point constant buffer index를 원본
                // PointBatch index와 일치시킨다.
                append_mesh_draws(
                    output,
                    scene,
                    definition.mesh,
                    data::EnumPointOrMatrix::POINT,
                    batch.instance_offset,
                    batch.instance_count,
                    batch_index,
                    bounds.points.batch_bounds[
                        batch_index],
                        bounds.points.batch_max_scale[
                            batch_index]);
            }
        }

        void append_point_draws(
            std::vector<data::DrawFinalGPUIndirect>& output,
            const scene::StaticScene& scene,
            const data::SceneBounds& bounds,
            const RendererOptions& options) {

            if (options.object_selection ==
                ObjectSelectionMode::DEMO_PYRAMID ||
                options.object_selection ==
                ObjectSelectionMode::DEMO_BASIC) {

                return;
            }

            const auto& components =
                scene.components;

            append_point_range(
                output,
                scene,
                bounds,
                components.anthurium.point_batches);

            append_point_range(
                output,
                scene,
                bounds,
                components.nettle.point_batches);

            append_point_range(
                output,
                scene,
                bounds,
                components.shrub_sorrel.point_batches);

            append_point_range(
                output,
                scene,
                bounds,
                components.shrub.point_batches);

            append_point_range(
                output,
                scene,
                bounds,
                components.grass_b.point_batches);

            append_point_range(
                output,
                scene,
                bounds,
                components.grass_a.point_batches);

            append_point_range(
                output,
                scene,
                bounds,
                components.pyramid_grass_b.point_batches);

            append_point_range(
                output,
                scene,
                bounds,
                components.pyramid_moss.point_batches);

            append_point_range(
                output,
                scene,
                bounds,
                components.queen_forest.point_batches);

            append_point_range(
                output,
                scene,
                bounds,
                components.river_forest.point_batches);

            append_point_range(
                output,
                scene,
                bounds,
                components.river_sapling.point_batches);

            append_point_range(
                output,
                scene,
                bounds,
                components.river_seedling.point_batches);
        }

        void append_static_instance(
            std::vector<data::DrawFinalGPUIndirect>& output,
            const scene::StaticScene& scene,
            const data::SceneBounds& bounds,
            std::uint32_t instance_index) {

            if (instance_index ==
                scene::StaticScene::INVALID_INDEX) {

                return;
            }

            const auto& instance =
                scene.static_mesh_instances[
                    instance_index];

            append_mesh_draws(
                output,
                scene,
                instance.mesh,
                data::EnumPointOrMatrix::MATRIX,
                instance_index,
                1,
                0,
                bounds.static_instances.bounds[
                    instance_index],
                    bounds.static_instances.max_scale[
                        instance_index]);
        }

        void append_static_range(
            std::vector<data::DrawFinalGPUIndirect>& output,
            const scene::StaticScene& scene,
            const data::SceneBounds& bounds,
            scene::StaticScene::IndexRange range) {

            for (std::uint32_t local_instance = 0;
                local_instance < range.count;
                ++local_instance) {

                append_static_instance(
                    output,
                    scene,
                    bounds,
                    range.offset + local_instance);
            }
        }

        void append_static_draws(
            std::vector<data::DrawFinalGPUIndirect>& output,
            const scene::StaticScene& scene,
            const data::SceneBounds& bounds) {

            const auto& components =
                scene.components;

            append_static_instance(
                output,
                scene,
                bounds,
                components.pyramid.instance);

            append_static_instance(
                output,
                scene,
                bounds,
                components.river.instance);

            append_static_instance(
                output,
                scene,
                bounds,
                components.creek.instance);

            append_static_instance(
                output,
                scene,
                bounds,
                components.banyan.instance);

            append_static_range(
                output,
                scene,
                bounds,
                components.terrain.extended);

            append_static_range(
                output,
                scene,
                bounds,
                components.terrain.cinematic);
        }

    } // namespace

    std::vector<data::DrawFinalGPUIndirect>
        SceneDrawBuilder::build(
            const scene::StaticScene& scene,
            const data::SceneBounds& bounds,
            const RendererOptions& options) {

        std::vector<data::DrawFinalGPUIndirect> result;

        append_point_draws(
            result,
            scene,
            bounds,
            options);

        append_static_draws(
            result,
            scene,
            bounds);

        return result;
    }

} // namespace fjr::render