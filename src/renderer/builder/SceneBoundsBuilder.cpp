#include "FastJungle/renderer/builder/SceneBoundsBuilder.hpp"

#include <DirectXMath.h>

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace fjr::render {
    namespace {

        [[nodiscard]] math::AABB transformed(
            const math::AABB& source,
            DirectX::FXMMATRIX transform) noexcept {

            auto result = source;
            result.transform(transform);
            return result;
        }

        [[nodiscard]] float maximum_scale(
            DirectX::FXMMATRIX transform) noexcept {

            DirectX::XMFLOAT4X4 matrix;
            DirectX::XMStoreFloat4x4(&matrix, transform);
            const auto length = [](float x, float y, float z) noexcept {
                return std::sqrt(x * x + y * y + z * z);
            };
            return std::max({
                length(matrix._11, matrix._12, matrix._13),
                length(matrix._21, matrix._22, matrix._23),
                length(matrix._31, matrix._32, matrix._33),
            });
        }

    } // namespace

    SceneBoundsBuilder SceneBoundsBuilder::build(
        const scene::StaticScene& scene) {

        SceneBoundsBuilder result;
        result.submesh_bounds.resize(scene.submeshes.size());
        result.mesh_bounds.resize(scene.meshes.size());
        result.instanced_definition_bounds.resize(
            scene.instanced_mesh_definitions.size());
        result.point_batch_bounds.resize(scene.point_batches.size());
        result.point_batch_max_scale.resize(scene.point_batches.size());
        result.static_instance_bounds.resize(
            scene.static_mesh_instances.size());
        result.static_instance_max_scale.resize(
            scene.static_mesh_instances.size());

        for (std::size_t mesh_index = 0; mesh_index < scene.meshes.size(); ++mesh_index) {
            const auto& mesh = scene.meshes[mesh_index];
            const auto& lod0 = scene.mesh_lods[mesh.lod_offset];
            auto& bounds = result.mesh_bounds[mesh_index];
            for (std::uint32_t local_submesh = 0;
                 local_submesh < lod0.submesh_count;
                 ++local_submesh) {
                const auto submesh_index =
                    static_cast<std::size_t>(lod0.submesh_offset) +
                    local_submesh;
                const auto& submesh = scene.submeshes[submesh_index];
                auto& submesh_bounds = result.submesh_bounds[submesh_index];
                for (std::uint32_t local_vertex = 0;
                     local_vertex < submesh.vertex_count;
                     ++local_vertex) {
                    submesh_bounds.merge(scene.vertices[
                        static_cast<std::size_t>(submesh.vertex_offset) +
                        local_vertex].position);
                }
                bounds.merge(submesh_bounds);
            }

            for (std::uint32_t local_lod = 1;
                 local_lod < mesh.lod_count;
                 ++local_lod) {
                const auto& lod = scene.mesh_lods[mesh.lod_offset + local_lod];
                for (std::uint32_t local_submesh = 0;
                     local_submesh < lod.submesh_count;
                     ++local_submesh) {
                    result.submesh_bounds[lod.submesh_offset + local_submesh] =
                        result.submesh_bounds[
                            lod0.submesh_offset + local_submesh];
                }
            }
        }

        for (std::size_t definition_index = 0;
             definition_index < scene.instanced_mesh_definitions.size();
             ++definition_index) {
            const auto& definition =
                scene.instanced_mesh_definitions[definition_index];
            result.instanced_definition_bounds[definition_index] =
                transformed(
                    result.mesh_bounds[definition.mesh],
                    DirectX::XMLoadFloat4x4(
                        &definition.local_transform));
        }

        for (std::size_t batch_index = 0;
             batch_index < scene.point_batches.size();
             ++batch_index) {
            const auto& batch = scene.point_batches[batch_index];
            auto& bounds = result.point_batch_bounds[batch_index];
            auto& max_scale = result.point_batch_max_scale[batch_index];
            const auto& definition =
                scene.instanced_mesh_definitions[batch.definition];
            const auto definition_local = DirectX::XMLoadFloat4x4(
                &definition.local_transform);
            const auto batch_world = DirectX::XMLoadFloat4x4(
                &batch.local_to_world);
            for (std::uint32_t local_instance = 0;
                 local_instance < batch.instance_count;
                 ++local_instance) {
                const auto& instance = scene.point_instances[
                    static_cast<std::size_t>(batch.instance_offset) +
                    local_instance];
                const auto scale = DirectX::XMMatrixScaling(
                    instance.scale.x,
                    instance.scale.y,
                    instance.scale.z);
                const auto rotation = DirectX::XMMatrixRotationQuaternion(
                    DirectX::XMLoadFloat4(&instance.orientation));
                const auto translation = DirectX::XMMatrixTranslation(
                    instance.position.x,
                    instance.position.y,
                    instance.position.z);
                const auto instance_world = DirectX::XMMatrixMultiply(
                    DirectX::XMMatrixMultiply(
                        scale,
                        rotation),
                    translation);
                // Apply the complete transform to the mesh AABB once. This
                // avoids compounding the looseness of intermediate AABBs.
                const auto world = DirectX::XMMatrixMultiply(
                    DirectX::XMMatrixMultiply(
                        definition_local,
                        instance_world),
                    batch_world);
                bounds.merge(transformed(
                    result.mesh_bounds[definition.mesh],
                    world));
                max_scale = std::max(max_scale, maximum_scale(world));
            }
            result.world_bounds.merge(bounds);
        }

        for (std::size_t instance_index = 0;
             instance_index < scene.static_mesh_instances.size();
             ++instance_index) {
            const auto& instance =
                scene.static_mesh_instances[instance_index];
            auto& bounds = result.static_instance_bounds[instance_index];
            bounds = transformed(
                result.mesh_bounds[instance.mesh],
                DirectX::XMLoadFloat4x4(&instance.world_transform));
            result.static_instance_max_scale[instance_index] = maximum_scale(
                DirectX::XMLoadFloat4x4(&instance.world_transform));
            result.world_bounds.merge(bounds);
        }

        return result;
    }

} // namespace fjr::render
