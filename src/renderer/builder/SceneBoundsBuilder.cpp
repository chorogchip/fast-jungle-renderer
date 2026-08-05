#include "FastJungle/renderer/builder/SceneBoundsBuilder.hpp"

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render {

    namespace {

        [[nodiscard]]
        math::AABB transformed(
            const math::AABB& source,
            DirectX::FXMMATRIX transform) noexcept {

            auto result = source;
            result.transform(transform);
            return result;
        }

        [[nodiscard]]
        float maximum_scale(
            DirectX::FXMMATRIX transform) noexcept {

            DirectX::XMFLOAT4X4 matrix;
            DirectX::XMStoreFloat4x4(&matrix, transform);

            const auto length = [](
                float x,
                float y,
                float z) noexcept {

                    return std::sqrt(x * x + y * y + z * z);
                };

            return std::max({
                length(matrix._11, matrix._12, matrix._13),
                length(matrix._21, matrix._22, matrix._23),
                length(matrix._31, matrix._32, matrix._33),
                });
        }

    } // namespace

    data::SceneBounds SceneBoundsBuilder::build(
        const scene::StaticScene& scene) {

        data::SceneBounds result;

        auto& geometry = result.geometry;
        auto& points = result.points;
        auto& static_instances = result.static_instances;

        geometry.submesh_bounds.resize(scene.submeshes.size());
        geometry.mesh_bounds.resize(scene.meshes.size());
        geometry.definition_bounds.resize(
            scene.instanced_mesh_definitions.size());
        geometry.definition_max_scale.resize(
            scene.instanced_mesh_definitions.size());

        points.batch_bounds.resize(scene.point_batches.size());
        points.batch_max_scale.resize(scene.point_batches.size());
        points.batch_transform_max_scale.resize(
            scene.point_batches.size());
        points.batch_cluster_ranges.resize(
            scene.point_batches.size());

        static_instances.bounds.resize(
            scene.static_mesh_instances.size());
        static_instances.max_scale.resize(
            scene.static_mesh_instances.size());

        // LOD0 geometry bounds
        for (std::size_t mesh_index = 0;
            mesh_index < scene.meshes.size();
            ++mesh_index) {

            const auto& mesh = scene.meshes[mesh_index];
            const auto& lod0 =
                scene.mesh_lods[mesh.lod_offset];

            auto& mesh_bounds =
                geometry.mesh_bounds[mesh_index];

            for (std::uint32_t local_submesh = 0;
                local_submesh < lod0.submesh_count;
                ++local_submesh) {

                const auto submesh_index =
                    static_cast<std::size_t>(
                        lod0.submesh_offset) +
                    local_submesh;

                const auto& submesh =
                    scene.submeshes[submesh_index];

                auto& submesh_bounds =
                    geometry.submesh_bounds[submesh_index];

                for (std::uint32_t local_vertex = 0;
                    local_vertex < submesh.vertex_count;
                    ++local_vertex) {

                    const auto vertex_index =
                        static_cast<std::size_t>(
                            submesh.vertex_offset) +
                        local_vertex;

                    submesh_bounds.merge(
                        scene.vertices[vertex_index].position);
                }

                mesh_bounds.merge(submesh_bounds);
            }

            // 모든 LOD submesh가 LOD0 vertex range를 공유한다.
            for (std::uint32_t local_lod = 1;
                local_lod < mesh.lod_count;
                ++local_lod) {

                const auto& lod = scene.mesh_lods[
                    static_cast<std::size_t>(
                        mesh.lod_offset) +
                        local_lod];

                for (std::uint32_t local_submesh = 0;
                    local_submesh < lod.submesh_count;
                    ++local_submesh) {

                    geometry.submesh_bounds[
                        static_cast<std::size_t>(
                            lod.submesh_offset) +
                            local_submesh] =
                        geometry.submesh_bounds[
                            static_cast<std::size_t>(
                                lod0.submesh_offset) +
                                local_submesh];
                }
            }
        }

        // Instanced definition bounds
        for (std::size_t definition_index = 0;
            definition_index <
            scene.instanced_mesh_definitions.size();
            ++definition_index) {

            const auto& definition =
                scene.instanced_mesh_definitions[
                    definition_index];

            const auto transform =
                DirectX::XMLoadFloat4x4(
                    &definition.local_transform);

            geometry.definition_bounds[definition_index] =
                transformed(
                    geometry.mesh_bounds[definition.mesh],
                    transform);

            geometry.definition_max_scale[definition_index] =
                maximum_scale(transform);
        }

        // PointBatch와 PointCluster bounds
        for (std::size_t batch_index = 0;
            batch_index < scene.point_batches.size();
            ++batch_index) {

            const auto& batch =
                scene.point_batches[batch_index];

            const auto& definition =
                scene.instanced_mesh_definitions[
                    batch.definition];

            auto& batch_bounds =
                points.batch_bounds[batch_index];

            auto& batch_max_scale =
                points.batch_max_scale[batch_index];

            const auto definition_local =
                DirectX::XMLoadFloat4x4(
                    &definition.local_transform);

            const auto batch_world =
                DirectX::XMLoadFloat4x4(
                    &batch.local_to_world);

            points.batch_transform_max_scale[batch_index] =
                maximum_scale(batch_world);

            auto& cluster_range =
                points.batch_cluster_ranges[batch_index];

            cluster_range.offset =
                static_cast<std::uint32_t>(
                    points.clusters.size());

            for (std::uint32_t local_begin = 0;
                local_begin < batch.instance_count;
                local_begin += data::Consts::PNT_CLUSTER_SZ) {

                auto& cluster =
                    points.clusters.emplace_back();

                cluster.point_batch_index =
                    static_cast<std::uint32_t>(
                        batch_index);

                cluster.instances.offset =
                    batch.instance_offset + local_begin;

                cluster.instances.count =
                    std::min(
                        data::Consts::PNT_CLUSTER_SZ,
                        batch.instance_count - local_begin);

                for (std::uint32_t local_instance = 0;
                    local_instance <
                    cluster.instances.count;
                    ++local_instance) {

                    const auto instance_index =
                        static_cast<std::size_t>(
                            cluster.instances.offset) +
                        local_instance;

                    const auto& instance =
                        scene.point_instances[
                            instance_index];

                    const auto scale =
                        DirectX::XMMatrixScaling(
                            instance.scale.x,
                            instance.scale.y,
                            instance.scale.z);

                    const auto rotation =
                        DirectX::XMMatrixRotationQuaternion(
                            DirectX::XMLoadFloat4(
                                &instance.orientation));

                    const auto translation =
                        DirectX::XMMatrixTranslation(
                            instance.position.x,
                            instance.position.y,
                            instance.position.z);

                    const auto instance_world =
                        scale * rotation * translation;

                    const auto world =
                        definition_local *
                        instance_world *
                        batch_world;

                    const auto instance_bounds =
                        transformed(
                            geometry.mesh_bounds[
                                definition.mesh],
                                world);

                    cluster.world_bounds.merge(
                        instance_bounds);

                    batch_max_scale =
                        std::max(
                            batch_max_scale,
                            maximum_scale(world));
                }

                batch_bounds.merge(
                    cluster.world_bounds);
            }

            cluster_range.count =
                static_cast<std::uint32_t>(
                    points.clusters.size()) -
                cluster_range.offset;

            result.world_bounds.merge(batch_bounds);
        }

        // Matrix instances
        for (std::size_t instance_index = 0;
            instance_index <
            scene.static_mesh_instances.size();
            ++instance_index) {

            const auto& instance =
                scene.static_mesh_instances[
                    instance_index];

            const auto world =
                DirectX::XMLoadFloat4x4(
                    &instance.world_transform);

            static_instances.bounds[instance_index] =
                transformed(
                    geometry.mesh_bounds[instance.mesh],
                    world);

            static_instances.max_scale[instance_index] =
                maximum_scale(world);

            result.world_bounds.merge(
                static_instances.bounds[instance_index]);
        }

        return result;
    }

} // namespace fjr::render