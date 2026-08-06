#include "FastJungle/renderer/builder/ScenePointResourceBuilder.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <tuple>

#include "FastJungle/core/util/EnumUtils.hpp"

namespace fjr::render {
    namespace {
        [[nodiscard]]
        data::StbufPointCluster convert_cluster(
            const data::SceneBounds::PointClusterBounds& source) {
            const auto center = source.world_bounds.get_center();
            const auto size = source.world_bounds.get_size();
            data::StbufPointCluster result;
            result.bounds_center = center;
            result.bounds_extent = {
                size.x * 0.5f,
                size.y * 0.5f,
                size.z * 0.5f,
            };
            result.point_mesh_batch_index = source.point_mesh_batch_index;
            result.instance_offset = source.instances.offset;
            result.instance_count = source.instances.count;
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
            static_cast<std::uint32_t>(scene.point_batches.size()) * data::Consts::LOD_CNT;
        result.definitions.resize(scene.point_batches.size());
        result.mesh_batches.resize(scene.point_batches.size());
        for (std::size_t batch_id = 0; batch_id < scene.point_batches.size(); ++batch_id) {
            const auto& batch = scene.point_batches[batch_id];
            const auto& mesh = scene.meshes[batch.mesh];
            const auto& local_bounds = bounds.points.local_bounds[batch_id];
            const auto& cluster_range = bounds.points.batch_cluster_ranges[batch_id];
            const auto center = local_bounds.get_center();
            const auto size = local_bounds.get_size();
            auto& definition = result.definitions[batch_id];
            definition.bounds_center_scale = {
                center.x,
                center.y,
                center.z,
                bounds.points.local_max_scale[batch_id],
            };
            definition.bounds_extent = {
                size.x * 0.5f,
                size.y * 0.5f,
                size.z * 0.5f,
                0.0f,
            };
            float* errors = &definition.lod_errors.x;
            for (std::uint32_t lod = 0; lod < data::Consts::LOD_CNT; ++lod) {
                errors[lod] = scene.mesh_lods[
                    static_cast<std::size_t>(mesh.lod_offset) + lod].max_deviation;
            }
            auto& mesh_batch = result.mesh_batches[batch_id];
            mesh_batch.local_transform = batch.local_transform;
            mesh_batch.mesh_index = batch.mesh;
            mesh_batch.first_bin =
                static_cast<std::uint32_t>(batch_id) * data::Consts::LOD_CNT;
            mesh_batch.first_cluster = cluster_range.offset;
            mesh_batch.cluster_count = cluster_range.count;
        }

        // Point clusters
        result.clusters.reserve(bounds.points.clusters.size());
        for (const auto& cluster : bounds.points.clusters)
            result.clusters.push_back(convert_cluster(cluster));

        // Static point draw templates
        std::array<std::uint32_t, data::Consts::PIPELINE_CNT> command_counts{};
        std::set<std::tuple<
            std::uint32_t,
            std::uint32_t,
            std::uint32_t,
            data::EnumPSOClass,
            std::uint32_t,
            std::uint32_t,
            std::int32_t>> template_keys;

        for (const auto& draw : draw_items) {
            if (draw.instnace_class != data::EnumPointOrMatrix::POINT) {
                continue;
            }
            // Alpha blend remains on the direct path.
            if (enm::has(draw.flags, data::EnumDrawCpuFlag::ALPHA_BLENDED)) {
                continue;
            }
            data::StbufPointDraw output;
            const auto batch_id = draw.offset_cbuf_transform;
            output.bin_index = batch_id * data::Consts::LOD_CNT + draw.lod_index;
            output.point_mesh_batch_index = batch_id;
            output.material_id = draw.constants.offset_material;
            output.pipeline_class = draw.pso_class;
            output.index_count = draw.count_index;
            output.first_index = draw.offset_index;
            output.base_vertex = static_cast<std::int32_t>(draw.offset_vertex);
            const auto key = std::tuple{
                output.point_mesh_batch_index,
                draw.lod_index,
                output.material_id,
                output.pipeline_class,
                output.index_count,
                output.first_index,
                output.base_vertex,
            };
            if (!template_keys.insert(key).second) {
                continue;
            }
            const auto class_id = static_cast<std::size_t>(output.pipeline_class);
            ++command_counts[class_id];
            result.draw_templates.push_back(output);
        }

        // Indirect command ranges grouped by pipeline class.
        std::uint32_t command_cursor = 0;
        for (std::size_t class_id = 0; class_id < data::Consts::PIPELINE_CNT; ++class_id) {
            auto& range = result.indirect_layout.class_ranges[class_id];
            range.first_command = command_cursor;
            range.max_command_count = command_counts[class_id];
            command_cursor += command_counts[class_id];
        }
        result.indirect_layout.total_command_capacity = command_cursor;
        return result;
    }
} // namespace fjr::render
