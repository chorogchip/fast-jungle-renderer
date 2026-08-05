#include "FastJungle/renderer/builder/ScenePointResourceBuilder.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace fjr::render {

    namespace {

        [[nodiscard]]
        bool is_alpha_blended(
            data::EnumDrawCpuFlag flags) noexcept {

            return (
                static_cast<std::uint32_t>(flags) &
                static_cast<std::uint32_t>(
                    data::EnumDrawCpuFlag::
                    ALPHA_BLENDED)) != 0;
        }

        [[nodiscard]]
        data::StbufPointCluster convert_cluster(
            const data::SceneBounds::
            PointClusterBounds& source) {

            const auto center =
                source.world_bounds.get_center();

            const auto size =
                source.world_bounds.get_size();

            data::StbufPointCluster result;

            result.bounds_center =
                center;

            result.bounds_extent = {
                size.x * 0.5f,
                size.y * 0.5f,
                size.z * 0.5f
            };

            result.point_batch_index =
                source.point_batch_index;

            result.instance_offset =
                source.instances.offset;

            result.instance_count =
                source.instances.count;

            return result;
        }

    } // namespace

    data::SceneResourcesTemp::PointRenderPlan
        ScenePointResourceBuilder::build(
            const scene::StaticScene& scene,
            const data::SceneBounds& bounds,
            std::span<
            const data::DrawFinalGPUIndirect>
            draw_items) {

        data::SceneResourcesTemp::PointRenderPlan result;

        result.bin_count =
            static_cast<std::uint32_t>(
                scene.point_batches.size()) *
            data::Consts::LOD_CNT;

        // Point definitions
        result.definitions.resize(
            scene.instanced_mesh_definitions.size());

        for (std::size_t definition_index = 0;
            definition_index <
            scene.instanced_mesh_definitions.size();
            ++definition_index) {

            const auto& definition =
                scene.instanced_mesh_definitions[
                    definition_index];

            const auto& mesh =
                scene.meshes[definition.mesh];

            const auto& definition_bounds =
                bounds.geometry.definition_bounds[
                    definition_index];

            const auto center =
                definition_bounds.get_center();

            const auto size =
                definition_bounds.get_size();

            auto& output =
                result.definitions[
                    definition_index];

            output.bounds_center_scale = {
                center.x,
                center.y,
                center.z,
                bounds.geometry.definition_max_scale[
                    definition_index]
            };

            output.bounds_extent = {
                size.x * 0.5f,
                size.y * 0.5f,
                size.z * 0.5f,
                0.0f
            };

            float* errors =
                &output.lod_errors.x;

            for (std::uint32_t lod_index = 0;
                lod_index < data::Consts::LOD_CNT;
                ++lod_index) {

                errors[lod_index] =
                    scene.mesh_lods[
                        static_cast<std::size_t>(
                            mesh.lod_offset) +
                            lod_index]
                    .max_deviation;
            }
        }

        // Point batches
        result.batches.resize(
            scene.point_batches.size());

        for (std::size_t batch_index = 0;
            batch_index < scene.point_batches.size();
            ++batch_index) {

            const auto& source =
                scene.point_batches[batch_index];

            const auto& definition =
                scene.instanced_mesh_definitions[
                    source.definition];

            const auto& cluster_range =
                bounds.points.batch_cluster_ranges[
                    batch_index];

            auto& output =
                result.batches[batch_index];

            output.part_local_transform =
                definition.local_transform;

            output.batch_local_to_world =
                source.local_to_world;

            output.definition_index =
                source.definition;

            output.first_bin =
                static_cast<std::uint32_t>(
                    batch_index) *
                data::Consts::LOD_CNT;

            output.first_cluster =
                cluster_range.offset;

            output.cluster_count =
                cluster_range.count;

            output.transform_max_scale =
                bounds.points
                .batch_transform_max_scale[
                    batch_index];
        }

        // Point clusters
        result.clusters.reserve(
            bounds.points.clusters.size());

        for (const auto& cluster :
            bounds.points.clusters) {

            result.clusters.push_back(
                convert_cluster(cluster));
        }

        // Static point draw templates
        std::array<
            std::uint32_t,
            data::Consts::PIPELINE_CNT>
            command_counts{};

        for (const auto& draw : draw_items) {

            if (draw.instnace_class !=
                data::EnumPointOrMatrix::POINT) {

                continue;
            }

            // Alpha blend는 현재 direct path에 남긴다.
            if (is_alpha_blended(draw.flags)) {
                continue;
            }

            data::StbufPointDraw output;

            const auto point_batch_index =
                draw.offset_cbuf_transform;

            output.bin_index =
                point_batch_index *
                data::Consts::LOD_CNT +
                draw.lod_index;

            output.point_batch_index =
                point_batch_index;

            output.material_id =
                draw.constants.offset_material;

            output.pipeline_class =
                draw.pso_class;

            output.index_count =
                draw.count_index;

            output.first_index =
                draw.offset_index;

            output.base_vertex =
                static_cast<std::int32_t>(
                    draw.offset_vertex);

            const auto class_index =
                static_cast<std::size_t>(
                    output.pipeline_class);

            ++command_counts[class_index];

            result.draw_templates.push_back(output);
        }

        // Pipeline class별 indirect command 구간
        std::uint32_t command_cursor = 0;

        for (std::size_t class_index = 0;
            class_index <
            data::Consts::PIPELINE_CNT;
            ++class_index) {

            auto& range =
                result.indirect_layout
                .class_ranges[class_index];

            range.first_command =
                command_cursor;

            range.max_command_count =
                command_counts[class_index];

            command_cursor +=
                command_counts[class_index];
        }

        result.indirect_layout
            .total_command_capacity =
            command_cursor;

        return result;
    }

} // namespace fjr::render