#include "FastJungle/renderer/builder/SceneDrawBuilder.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

#include "FastJungle/core/util/EnumUtils.hpp"

namespace fjr::render {

    namespace {
        [[nodiscard]]
        data::EnumRasterClass select_raster_class(
            scene::StaticScene::EnumSubmeshFlag flags) noexcept {
            return enm::has(
                flags,
                scene::StaticScene::EnumSubmeshFlag::ALPHA_TESTED)
                ? data::EnumRasterClass::ALPHA_TESTED_DOUBLE_SIDED
                : data::EnumRasterClass::OPAQUE_SINGLE_SIDED;
        }

        void append_mesh_draws(
            data::SceneDraws& output,
            const scene::StaticScene& scene,
            std::uint32_t mesh_index,
            data::EnumInstanceKind instance_kind,
            scene::StaticScene::EnumPointCategory point_category,
            std::uint32_t instance_offset,
            std::uint32_t instance_count,
            std::uint32_t transform_index,
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

                    const auto raster_class =
                        select_raster_class(source.flags);
                    data::StbufDrawMetadata metadata;
                    metadata.material_id =
                        source.material == scene::StaticScene::INVALID_INDEX
                        ? static_cast<std::uint32_t>(scene.materials.size())
                        : source.material;
                    metadata.transform_index = transform_index;
                    metadata.index_count = source.index_count;
                    metadata.first_index = source.index_offset;
                    metadata.base_vertex =
                        static_cast<std::int32_t>(source.vertex_offset);
                    metadata.instance_kind = instance_kind;
                    metadata.raster_class = raster_class;

                    data::DrawFinalGPUIndirect draw;
                    draw.draw_id = static_cast<std::uint32_t>(
                        output.draw_metadata.size());
                    draw.instance_offset = instance_offset;
                    draw.instance_kind = instance_kind;
                    draw.raster_class = raster_class;
                    draw.point_category = point_category;
                    draw.offset_index = source.index_offset;
                    draw.offset_vertex = source.vertex_offset;
                    draw.count_index = source.index_count;
                    draw.count_instance = instance_count;
                    draw.lod_index = lod;
                    draw.world_bounds = world_bounds;
                    draw.world_scale = world_scale;
                    draw.lod_error = lod_data.max_deviation;
                    draw.next_lod_error = next_lod_error;
                    output.draw_metadata.push_back(metadata);
                    output.draw_items.push_back(draw);
                }
            }
        }

        void append_point_cluster(
            data::SceneDraws& output,
            const scene::StaticScene& scene,
            const data::SceneBounds::PointClusterBounds& cluster) {
            const auto batch_index = cluster.point_mesh_batch_index;
            const auto& batch = scene.point_batches[batch_index];
            append_mesh_draws(
                output,
                scene,
                batch.mesh,
                data::EnumInstanceKind::POINT,
                cluster.category,
                cluster.instances.offset,
                cluster.instances.count,
                batch_index,
                cluster.world_bounds,
                cluster.world_max_scale);
        }

        void append_point_draws(
            data::SceneDraws& output,
            const scene::StaticScene& scene,
            const data::SceneBounds& bounds) {
            for (const auto& cluster : bounds.points.clusters) {
                append_point_cluster(output, scene, cluster);
            }
        }

        void append_static_instance(
            data::SceneDraws& output,
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
                data::EnumInstanceKind::MATRIX,
                scene::StaticScene::EnumPointCategory::COUNT,
                instance_index,
                1,
                data::Consts::IND_ERR,
                bounds.static_instances.bounds[instance_index],
                bounds.static_instances.max_scale[instance_index]);
        }

        void append_static_range(
            data::SceneDraws& output,
            const scene::StaticScene& scene,
            const data::SceneBounds& bounds,
            scene::StaticScene::IndexRange range) {
            for (std::uint32_t instance = 0; instance < range.count; ++instance) {
                append_static_instance(output, scene, bounds, range.offset + instance);
            }
        }

        void append_static_draws(
            data::SceneDraws& output,
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
        append_point_draws(result, scene, bounds);
        append_static_draws(result, scene, bounds);

        std::map<std::tuple<
            std::uint32_t,
            std::uint32_t,
            std::uint32_t,
            std::uint32_t,
            std::int32_t,
            data::EnumInstanceKind,
            data::EnumRasterClass>, std::uint32_t> metadata_ids;
        std::vector<data::StbufDrawMetadata> unique_metadata;
        for (auto& draw : result.draw_items) {
            const auto& source = result.draw_metadata[draw.draw_id];
            const auto key = std::tuple{
                source.material_id,
                source.transform_index,
                source.index_count,
                source.first_index,
                source.base_vertex,
                source.instance_kind,
                source.raster_class,
            };
            const auto [entry, inserted] = metadata_ids.emplace(
                key,
                static_cast<std::uint32_t>(unique_metadata.size()));
            if (inserted) {
                unique_metadata.push_back(source);
            }
            draw.draw_id = entry->second;
        }
        result.draw_metadata = std::move(unique_metadata);

        std::erase_if(result.draw_items, [&](const data::DrawFinalGPUIndirect& draw) {
            if (draw.instance_kind == data::EnumInstanceKind::POINT) {
                if (draw.point_category == scene::StaticScene::EnumPointCategory::RIVER_SEEDLING)
                    return !options.objects.river_seedling;
                if (draw.point_category == scene::StaticScene::EnumPointCategory::RIVER_FOREST)
                    return !options.objects.river_forest;
                if (draw.point_category == scene::StaticScene::EnumPointCategory::PYRAMID_MOSS)
                    return !options.objects.pyramid_moss;
                return !options.objects.other_foliage;
            }

            const bool is_terrain =
                scene.components.terrain.extended.contains(draw.instance_offset) ||
                scene.components.terrain.cinematic.contains(draw.instance_offset);
            return is_terrain ? !options.objects.terrain : !options.objects.other;
        });
        return result;
    }

} // namespace fjr::render
