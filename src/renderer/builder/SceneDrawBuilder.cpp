#include "FastJungle/renderer/builder/SceneDrawBuilder.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "FastJungle/core/util/EnumUtils.hpp"

namespace fjr::render {

    namespace {
        [[nodiscard]]
        data::EnumPSOClass select_pso_class(
            scene::StaticScene::EnumSubmeshFlag flags) noexcept {
            return enm::has(
                flags,
                scene::StaticScene::EnumSubmeshFlag::DOUBLE_SIDED)
                ? data::EnumPSOClass::DOUBLE_SIDED
                : data::EnumPSOClass::SINGLE_SIDED;
        }

        [[nodiscard]]
        data::EnumDrawCpuFlag select_draw_flags(
            scene::StaticScene::EnumSubmeshFlag flags) noexcept {
            std::uint32_t value =
                static_cast<std::uint32_t>(
                    data::EnumDrawCpuFlag::DEFAULT);
            if (enm::has(
                flags,
                scene::StaticScene::EnumSubmeshFlag::DOUBLE_SIDED)) {
                value |= static_cast<std::uint32_t>(
                    data::EnumDrawCpuFlag::DOUBLE_SIDED);
            }

            if (enm::has(
                flags,
                scene::StaticScene::EnumSubmeshFlag::ALPHA_BLENDED)) {
                value |= static_cast<std::uint32_t>(
                    data::EnumDrawCpuFlag::ALPHA_BLENDED);
            }
            return static_cast<data::EnumDrawCpuFlag>(value);
        }

        void append_mesh_draws(
            std::vector<data::DrawFinalGPUIndirect>& output,
            const scene::StaticScene& scene,
            std::uint32_t mesh_index,
            data::EnumPointOrMatrix instance_class,
            scene::StaticScene::EnumPointCategory point_category,
            std::uint32_t instance_offset,
            std::uint32_t instance_count,
            std::uint32_t transform_constant_index,
            const math::AABB& world_bounds,
            float world_scale) {
            if (instance_count == 0) {
                return;
            }
            const auto& mesh = scene.meshes[mesh_index];

            for (std::uint32_t lod = 0; lod < mesh.lod_count; ++lod) {
                const auto lod_index = static_cast<std::size_t>(mesh.lod_offset) + lod;
                const auto& lod_data = scene.mesh_lods[lod_index];
                const float next_lod_error =
                    lod + 1 < mesh.lod_count
                    ? scene.mesh_lods[
                        lod_index + 1].max_deviation
                    : std::numeric_limits<float>::infinity();

                for (std::uint32_t submesh = 0; submesh < lod_data.submesh_count; ++submesh) {
                    const auto submesh_index =
                        static_cast<std::size_t>(lod_data.submesh_offset) + submesh;
                    const auto& source = scene.submeshes[submesh_index];
                    if (source.index_count == 0) {
                        continue;
                    }

                    data::DrawFinalGPUIndirect draw;
                    draw.constants.offset_instance = instance_offset;
                    draw.constants.offset_material =
                        source.material == scene::StaticScene::INVALID_INDEX
                        ? static_cast<std::uint32_t>(scene.materials.size())
                        : source.material;
                    draw.instnace_class = instance_class;
                    draw.pso_class = select_pso_class(source.flags);
                    draw.flags = select_draw_flags(source.flags);
                    draw.point_category = point_category;
                    draw.offset_cbuf_transform = transform_constant_index;
                    draw.offset_index = source.index_offset;
                    draw.offset_vertex = source.vertex_offset;
                    draw.count_index = source.index_count;
                    draw.count_instance = instance_count;
                    draw.lod_index = lod;
                    draw.world_bounds = world_bounds;
                    draw.world_scale = world_scale;
                    draw.lod_error = lod_data.max_deviation;
                    draw.next_lod_error = next_lod_error;
                    output.push_back(draw);
                }
            }
        }

        void append_point_cluster(
            std::vector<data::DrawFinalGPUIndirect>& output,
            const scene::StaticScene& scene,
            const data::SceneBounds::PointClusterBounds& cluster) {
            const auto batch_index = cluster.point_mesh_batch_index;
            const auto& batch = scene.point_batches[batch_index];
            append_mesh_draws(
                output,
                scene,
                batch.mesh,
                data::EnumPointOrMatrix::POINT,
                cluster.category,
                cluster.instances.offset,
                cluster.instances.count,
                batch_index,
                cluster.world_bounds,
                cluster.world_max_scale);
        }

        void append_point_draws(
            std::vector<data::DrawFinalGPUIndirect>& output,
            const scene::StaticScene& scene,
            const data::SceneBounds& bounds) {
            for (const auto& cluster : bounds.points.clusters) {
                append_point_cluster(output, scene, cluster);
            }
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
            const auto& instance = scene.static_mesh_instances[instance_index];
            append_mesh_draws(
                output,
                scene,
                instance.mesh,
                data::EnumPointOrMatrix::MATRIX,
                scene::StaticScene::EnumPointCategory::COUNT,
                instance_index,
                1,
                0,
                bounds.static_instances.bounds[instance_index],
                bounds.static_instances.max_scale[instance_index]);
        }

        void append_static_range(
            std::vector<data::DrawFinalGPUIndirect>& output,
            const scene::StaticScene& scene,
            const data::SceneBounds& bounds,
            scene::StaticScene::IndexRange range) {
            for (std::uint32_t instance = 0; instance < range.count; ++instance) {
                append_static_instance(output, scene, bounds, range.offset + instance);
            }
        }

        void append_static_draws(
            std::vector<data::DrawFinalGPUIndirect>& output,
            const scene::StaticScene& scene,
            const data::SceneBounds& bounds) {
            const auto& components = scene.components;
            append_static_instance(output, scene, bounds, components.pyramid.instance);
            append_static_instance(output, scene, bounds, components.river.instance);
            append_static_instance(output, scene, bounds, components.creek.instance);
            append_static_instance(output, scene, bounds, components.banyan.instance);
            append_static_range(output, scene, bounds, components.terrain.extended);
            append_static_range(output, scene, bounds, components.terrain.cinematic);
        }

    } // namespace

    data::SceneDraws SceneDrawBuilder::build(
        const scene::StaticScene& scene,
        const data::SceneBounds& bounds,
        const RendererOptions& options) {
        data::SceneDraws result;
        append_point_draws(result.draw_items, scene, bounds);
        append_static_draws(result.draw_items, scene, bounds);
        std::erase_if(result.draw_items, [&](const data::DrawFinalGPUIndirect& draw) {
            if (draw.instnace_class == data::EnumPointOrMatrix::POINT) {
                if (draw.point_category == scene::StaticScene::EnumPointCategory::RIVER_SEEDLING)
                    return !options.objects.river_seedling;
                if (draw.point_category == scene::StaticScene::EnumPointCategory::RIVER_FOREST)
                    return !options.objects.river_forest;
                if (draw.point_category == scene::StaticScene::EnumPointCategory::PYRAMID_MOSS)
                    return !options.objects.pyramid_moss;
                return !options.objects.other_foliage;
            }

            const bool is_terrain =
                scene.components.terrain.extended.contains(draw.constants.offset_instance) ||
                scene.components.terrain.cinematic.contains(draw.constants.offset_instance);
            return is_terrain ? !options.objects.terrain : !options.objects.other;
        });
        return result;
    }

} // namespace fjr::render
