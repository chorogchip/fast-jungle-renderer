#include "FastJungle/renderer/SceneDerivedData.hpp"

#include <DirectXMath.h>

#include <cstddef>
#include <cstdint>

namespace fjr::render {
    namespace {

        [[nodiscard]] math::AABB transformed(
            const math::AABB& source,
            DirectX::FXMMATRIX transform) noexcept {

            auto result = source;
            result.transform(transform);
            return result;
        }

    } // namespace

    SceneDerivedData SceneDerivedData::build(
        const scene::StaticScene& scene) {

        SceneDerivedData result;
        result.submesh_bounds.resize(scene.submeshes.size());
        result.mesh_bounds.resize(scene.meshes.size());
        result.prototype_bounds.resize(scene.prototypes.size());
        result.point_batch_bounds.resize(scene.point_batches.size());
        result.matrix_batch_bounds.resize(scene.matrix_batches.size());

        for (std::size_t submesh_index = 0;
             submesh_index < scene.submeshes.size();
             ++submesh_index) {
            const auto& submesh = scene.submeshes[submesh_index];
            auto& bounds = result.submesh_bounds[submesh_index];
            for (std::uint32_t local_vertex = 0;
                 local_vertex < submesh.vertex_count;
                 ++local_vertex) {
                bounds.merge(scene.vertices[
                    static_cast<std::size_t>(submesh.vertex_offset) +
                    local_vertex].position);
            }
        }

        for (std::size_t mesh_index = 0;
             mesh_index < scene.meshes.size();
             ++mesh_index) {
            const auto& mesh = scene.meshes[mesh_index];
            auto& bounds = result.mesh_bounds[mesh_index];
            for (std::uint32_t local_submesh = 0;
                 local_submesh < mesh.submesh_count;
                 ++local_submesh) {
                bounds.merge(result.submesh_bounds[
                    static_cast<std::size_t>(mesh.submesh_offset) +
                    local_submesh]);
            }
        }

        for (std::size_t prototype_index = 0;
             prototype_index < scene.prototypes.size();
             ++prototype_index) {
            const auto& prototype = scene.prototypes[prototype_index];
            auto& bounds = result.prototype_bounds[prototype_index];
            for (std::uint32_t local_part = 0;
                 local_part < prototype.part_count;
                 ++local_part) {
                const auto& part = scene.prototype_parts[
                    static_cast<std::size_t>(prototype.part_offset) +
                    local_part];
                bounds.merge(transformed(
                    result.mesh_bounds[part.mesh],
                    DirectX::XMLoadFloat4x4(&part.local_transform)));
            }
        }

        for (std::size_t batch_index = 0;
             batch_index < scene.point_batches.size();
             ++batch_index) {
            const auto& batch = scene.point_batches[batch_index];
            auto& bounds = result.point_batch_bounds[batch_index];
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
                const auto world = DirectX::XMMatrixMultiply(
                    DirectX::XMMatrixMultiply(
                        DirectX::XMMatrixMultiply(scale, rotation),
                        translation),
                    batch_world);
                bounds.merge(transformed(
                    result.prototype_bounds[batch.prototype],
                    world));
            }
            result.world_bounds.merge(bounds);
        }

        for (std::size_t batch_index = 0;
             batch_index < scene.matrix_batches.size();
             ++batch_index) {
            const auto& batch = scene.matrix_batches[batch_index];
            auto& bounds = result.matrix_batch_bounds[batch_index];
            for (std::uint32_t local_instance = 0;
                 local_instance < batch.instance_count;
                 ++local_instance) {
                const auto& instance = scene.matrix_instances[
                    static_cast<std::size_t>(batch.instance_offset) +
                    local_instance];
                bounds.merge(transformed(
                    result.prototype_bounds[batch.prototype],
                    DirectX::XMLoadFloat4x4(&instance.transform)));
            }
            result.world_bounds.merge(bounds);
        }

        return result;
    }

} // namespace fjr::render
