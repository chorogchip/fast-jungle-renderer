#include "FastJungle/renderer/builder/SceneBoundsBuilder.hpp"

#include <DirectXMath.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "FastJungle/core/math/Matrix.hpp"

namespace fjr::render {

    data::SceneBounds SceneBoundsBuilder::build(const scene::StaticScene& scene,
        const data::PointCullingData& point_culling) {
        data::SceneBounds result;
        auto& geometry = result.geometry;
        auto& points = result.points;
        auto& static_instances = result.static_instances;
        geometry.submesh_bounds.resize(scene.submeshes.size());
        geometry.mesh_bounds.resize(scene.meshes.size());
        points.local_bounds.resize(scene.point_batches.size());
        points.local_max_scale.resize(scene.point_batches.size());
        points.batch_bounds.resize(scene.point_batches.size());
        points.batch_max_scale.resize(scene.point_batches.size());
        points.batch_cluster_ranges.resize(
            scene.point_batches.size());
        static_instances.bounds.resize(
            scene.static_mesh_instances.size());
        static_instances.max_scale.resize(
            scene.static_mesh_instances.size());

        // LOD0 geometry bounds
        for (std::size_t mesh_id = 0; mesh_id < scene.meshes.size(); ++mesh_id) {
            const auto& mesh = scene.meshes[mesh_id];
            const auto& lod0 = scene.mesh_lods[mesh.lod_offset];
            auto& mesh_bounds = geometry.mesh_bounds[mesh_id];

            for (std::uint32_t submesh = 0; submesh < lod0.submesh_count; ++submesh) {
                const auto submesh_id =
                    static_cast<std::size_t>(lod0.submesh_offset) + submesh;
                const auto& source = scene.submeshes[submesh_id];
                auto& submesh_bounds = geometry.submesh_bounds[submesh_id];

                for (std::uint32_t vertex = 0; vertex < source.vertex_count; ++vertex) {
                    const auto vertex_id =
                        static_cast<std::size_t>(source.vertex_offset) + vertex;
                    submesh_bounds.merge(scene.vertices[vertex_id].position);
                }
                mesh_bounds.merge(submesh_bounds);
            }

            // 모든 LOD submesh가 LOD0 vertex range를 공유한다.
            for (std::uint32_t lod = 1; lod < mesh.lod_count; ++lod) {
                const auto& lod_data = scene.mesh_lods[
                    static_cast<std::size_t>(mesh.lod_offset) + lod];
                for (std::uint32_t submesh = 0; submesh < lod_data.submesh_count; ++submesh) {
                    geometry.submesh_bounds[
                        static_cast<std::size_t>(lod_data.submesh_offset) + submesh] =
                        geometry.submesh_bounds[
                            static_cast<std::size_t>(lod0.submesh_offset) + submesh];
                }
            }
        }

        // PointBatch와 PointCluster bounds
        for (std::size_t batch_id = 0; batch_id < scene.point_batches.size(); ++batch_id) {
            const auto& batch = scene.point_batches[batch_id];
            const auto batch_local = DirectX::XMLoadFloat4x4(&batch.local_transform);
            points.local_bounds[batch_id] =
                geometry.mesh_bounds[batch.mesh].transformed(batch_local);
            points.local_max_scale[batch_id] = math::Matrix::maximum_scale(batch_local);
        }

        std::size_t cluster_id = 0;

        // Keep GPU cluster ranges contiguous per mesh batch while preserving
        // the instance order selected by the user callback.
        for (std::size_t batch_id = 0; batch_id < scene.point_batches.size(); ++batch_id) {
            const auto& batch = scene.point_batches[batch_id];
            auto& batch_bounds = points.batch_bounds[batch_id];
            auto& batch_max_scale = points.batch_max_scale[batch_id];
            const auto batch_local = DirectX::XMLoadFloat4x4(&batch.local_transform);
            auto& cluster_range = points.batch_cluster_ranges[batch_id];
            cluster_range.offset = static_cast<std::uint32_t>(points.clusters.size());

            while (cluster_id < point_culling.batches.size() &&
                point_culling.batches[cluster_id].point_batch_index ==
                static_cast<std::uint32_t>(batch_id)) {
                const auto& source_batch = point_culling.batches[cluster_id];
                auto& cluster = points.clusters.emplace_back();
                cluster.point_mesh_batch_index = static_cast<std::uint32_t>(batch_id);
                cluster.category = batch.category;
                cluster.instances = source_batch.instance_order_id;

                for (std::uint32_t instance = 0; instance < cluster.instances.count; ++instance) {
                    const auto order =
                        static_cast<std::size_t>(cluster.instances.offset) + instance;
                    const auto source_id = point_culling.instance_order[order];
                    const auto& source = scene.point_instances[source_id];
                    const auto scale = DirectX::XMMatrixScaling(
                        source.scale.x, source.scale.y, source.scale.z);
                    const auto rotation = DirectX::XMMatrixRotationQuaternion(
                        DirectX::XMLoadFloat4(&source.orientation));
                    const auto translation = DirectX::XMMatrixTranslation(
                        source.position.x, source.position.y, source.position.z);
                    const auto world = batch_local * (scale * rotation * translation);
                    const auto instance_bounds =
                        geometry.mesh_bounds[batch.mesh].transformed(world);
                    cluster.world_bounds.merge(instance_bounds);

                    const float instance_max_scale = math::Matrix::maximum_scale(world);
                    cluster.world_max_scale = std::max(
                        cluster.world_max_scale, instance_max_scale);
                    batch_max_scale = std::max(batch_max_scale, instance_max_scale);
                }
                batch_bounds.merge(cluster.world_bounds);
                ++cluster_id;
            }
            cluster_range.count =
                static_cast<std::uint32_t>(points.clusters.size()) - cluster_range.offset;
            result.world_bounds.merge(batch_bounds);
        }

        // Matrix instances
        for (std::size_t instance_id = 0; instance_id < scene.static_mesh_instances.size(); ++instance_id) {
            const auto& instance = scene.static_mesh_instances[instance_id];
            const auto world = DirectX::XMLoadFloat4x4(&instance.world_transform);
            static_instances.bounds[instance_id] =
                geometry.mesh_bounds[instance.mesh].transformed(world);
            static_instances.max_scale[instance_id] = math::Matrix::maximum_scale(world);
            result.world_bounds.merge(static_instances.bounds[instance_id]);
        }
        return result;
    }

} // namespace fjr::render
