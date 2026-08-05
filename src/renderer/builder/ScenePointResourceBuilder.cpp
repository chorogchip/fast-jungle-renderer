
#include "FastJungle/renderer/builder/ScenePointResourceBuilder.hpp"

namespace fjr::render {

    namespace {

        PointClusterGpu convert_cluster(
            const PointClusterCpu& source) {

            const auto center = source.world_bounds.get_center();
            const auto size = source.world_bounds.get_size();

            PointClusterGpu result;
            result.bounds_center = center;
            result.bounds_extent = {
                size.x * 0.5f,
                size.y * 0.5f,
                size.z * 0.5f
            };
            result.point_batch_index =
                source.point_batch_index;
            result.instance_offset =
                source.instance_offset;
            result.instance_count =
                source.instance_count;
            return result;
        }

        bool is_alpha_blended(
            scene::StaticScene::EnumSubmeshFlag flags) {

            return (
                static_cast<std::uint32_t>(flags) &
                static_cast<std::uint32_t>(
                    scene::StaticScene::EnumSubmeshFlag::
                    ALPHA_BLENDED)) != 0;
        }

        std::uint32_t pipeline_class(
            scene::StaticScene::EnumSubmeshFlag flags) {

            const bool double_sided =
                (static_cast<std::uint32_t>(flags) &
                    static_cast<std::uint32_t>(
                        scene::StaticScene::EnumSubmeshFlag::
                        DOUBLE_SIDED)) != 0;

            return double_sided ? 1u : 0u;
        }

    }

	ScenePointResources ScenePointResourceBuilder::build(
		const scene::StaticScene& scene,
		const SceneBoundsBuilder& bounds,
		std::span<const SceneDrawItem> draw_items) {

        ScenePointResources result;

        result.bin_count =
            static_cast<std::uint32_t>(
                scene.point_batches.size()) *
            POINT_LOD_COUNT;

        // Definitions
        result.definitions.resize(
            scene.instanced_mesh_definitions.size());

        for (std::size_t index = 0;
            index < result.definitions.size();
            ++index) {

            const auto& definition =
                scene.instanced_mesh_definitions[index];
            const auto& mesh =
                scene.meshes[definition.mesh];
            const auto& aabb =
                bounds.instanced_definition_bounds[index];

            auto& destination =
                result.definitions[index];

            const auto center = aabb.get_center();
            const auto size = aabb.get_size();

            destination.bounds_center_scale = {
                center.x,
                center.y,
                center.z,
                bounds.instanced_definition_max_scale[index]
            };

            destination.bounds_extent = {
                size.x * 0.5f,
                size.y * 0.5f,
                size.z * 0.5f,
                0.0f
            };

            float* errors =
                &destination.lod_errors.x;

            for (std::uint32_t lod = 0;
                lod < POINT_LOD_COUNT;
                ++lod) {
                errors[lod] =
                    scene.mesh_lods[
                        mesh.lod_offset + lod].max_deviation;
            }
        }

        // Batches
        result.batches.resize(
            scene.point_batches.size());

        for (std::size_t index = 0;
            index < result.batches.size();
            ++index) {

            const auto& source =
                scene.point_batches[index];
            const auto& definition =
                scene.instanced_mesh_definitions[
                    source.definition];
            const auto& cluster_range =
                bounds.point_batch_cluster_ranges[index];

            auto& destination =
                result.batches[index];

            destination.part_local_transform =
                definition.local_transform;
            destination.batch_local_to_world =
                source.local_to_world;

            destination.indices = {
                source.definition,
                static_cast<std::uint32_t>(index) *
                    POINT_LOD_COUNT,
                cluster_range.offset,
                cluster_range.count
            };

            destination.culling.x =
                bounds.point_batch_transform_max_scale[index];
        }

        // Clusters
        result.clusters.reserve(
            bounds.point_clusters.size());

        for (const auto& source : bounds.point_clusters) {
            result.clusters.push_back(
                convert_cluster(source));
        }

        // Draw templates
        for (const auto& draw : draw_items) {
            if (draw.instance_kind !=
                SceneResources::InstanceKind::POINT) {
                continue;
            }

            // M1에서는 alpha blend를 기존 CPU direct path에 남긴다.
            if (is_alpha_blended(draw.flags)) {
                continue;
            }

            PointDrawTemplateGpu destination;
            destination.bin_index =
                draw.instance_bin_index;
            destination.point_batch_index =
                draw.bounds_index;
            destination.material_id =
                draw.constants.material_id;
            destination.pipeline_class =
                pipeline_class(draw.flags);

            destination.index_count =
                draw.index_count;
            destination.first_index =
                draw.first_index;
            destination.base_vertex =
                draw.base_vertex;

            ++result.command_class_capacities[
                destination.pipeline_class];

            result.draw_templates.push_back(destination);
        }

        result.command_class_bases[0] = 0;
        result.command_class_bases[1] = result.command_class_capacities[0];

        return result;

	}
}