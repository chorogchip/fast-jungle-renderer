#include "FastJungle/scene/StaticSceneValidation.hpp"

#include "FastJungle/core/util/Logger.hpp"

#include <algorithm>

namespace fjr::scene {

    void StaticSceneValidator::validate(const StaticScene& scene) {
        validate(scene, scene.texture_data.size());
    }

    void StaticSceneValidator::validate(
        const StaticScene& scene,
        std::uint64_t texture_payload_size) {

        if (scene.strings.empty() || scene.strings.front() != '\0') {
            log::Logger::g_logger
                << "Invalid StaticScene string table.\n";
            log::Logger::g_logger.abort();
        }

        for (const auto& group : scene.source_groups) {
            require_string(scene, group.name, "source group name");
        }
        for (const auto& layer : scene.source_layers) {
            require_string(scene, layer.name, "source layer name");
            require_string(scene, layer.path, "source layer path");
            require_index(
                layer.group,
                scene.source_groups.size(),
                "source layer group");
        }

        for (const auto& texture : scene.textures) {
            require_string(scene, texture.name, "texture name");
            require_range(
                texture.mip_offset,
                texture.mip_count,
                scene.texture_mips.size(),
                "texture mip");
            require_range(
                texture.data_byte_offset,
                texture.data_size,
                texture_payload_size,
                "texture data",
                StaticScene::INVALID_INDEX_64);

            for (std::uint32_t index = 0;
                 index < texture.mip_count;
                 ++index) {
                const auto& mip = scene.texture_mips[
                    texture.mip_offset + index];
                require_range(
                    mip.data_byte_offset_local,
                    mip.slice_pitch,
                    texture.data_size,
                    "texture mip data",
                    StaticScene::INVALID_INDEX_64);
            }
        }

        for (const auto& binding : scene.texture_bindings) {
            require_index(
                binding.texture,
                scene.textures.size(),
                "texture binding texture");
            require_index(
                binding.sampler,
                scene.samplers.size(),
                "texture binding sampler");
        }

        const auto validate_optional_binding = [&scene](
            std::uint32_t binding,
            std::string_view subject) {
            if (binding != StaticScene::INVALID_INDEX) {
                require_index(
                    binding,
                    scene.texture_bindings.size(),
                    subject);
            }
        };
        for (const auto& material : scene.materials) {
            require_string(scene, material.name, "material name");
            validate_optional_binding(
                material.texture_binding_base_color,
                "material base-color binding");
            validate_optional_binding(
                material.texture_binding_normal,
                "material normal binding");
            validate_optional_binding(
                material.texture_binding_roughness,
                "material roughness binding");
            validate_optional_binding(
                material.texture_binding_opacity,
                "material opacity binding");
            validate_optional_binding(
                material.texture_binding_emissive,
                "material emissive binding");
        }

        for (const auto& submesh : scene.submeshes) {
            require_string(scene, submesh.name, "submesh name");
            require_range(
                submesh.vertex_offset,
                submesh.vertex_count,
                scene.vertices.size(),
                "submesh vertex");
            require_range(
                submesh.index_offset,
                submesh.index_count,
                scene.indices.size(),
                "submesh index");
            require_index(
                submesh.material,
                scene.materials.size(),
                "submesh material");
        }

        for (const auto& mesh : scene.meshes) {
            require_string(scene, mesh.name, "mesh name");
            require_range(
                mesh.submesh_offset,
                mesh.submesh_count,
                scene.submeshes.size(),
                "mesh submesh");
        }

        for (const auto& part : scene.prototype_parts) {
            require_index(
                part.mesh,
                scene.meshes.size(),
                "prototype part mesh");
        }
        for (const auto& prototype : scene.prototypes) {
            require_string(scene, prototype.name, "prototype name");
            require_range(
                prototype.part_offset,
                prototype.part_count,
                scene.prototype_parts.size(),
                "prototype part");
        }

        for (const auto& batch : scene.point_batches) {
            require_string(scene, batch.name, "point batch name");
            require_string(
                scene,
                batch.source_prim_path,
                "point batch source prim path");
            require_index(
                batch.source_layer,
                scene.source_layers.size(),
                "point batch source layer");
            require_index(
                batch.prototype,
                scene.prototypes.size(),
                "point batch prototype");
            require_range(
                batch.instance_offset,
                batch.instance_count,
                scene.point_instances.size(),
                "point batch instance");
        }

        for (const auto& batch : scene.matrix_batches) {
            require_string(scene, batch.name, "matrix batch name");
            require_string(
                scene,
                batch.source_prim_path,
                "matrix batch source prim path");
            require_index(
                batch.source_layer,
                scene.source_layers.size(),
                "matrix batch source layer");
            require_index(
                batch.prototype,
                scene.prototypes.size(),
                "matrix batch prototype");
            require_range(
                batch.instance_offset,
                batch.instance_count,
                scene.matrix_instances.size(),
                "matrix batch instance");
        }

        if (scene.camera.name != StaticScene::INVALID_INDEX) {
            require_string(scene, scene.camera.name, "camera name");
        }
        if (scene.environment_light.name != StaticScene::INVALID_INDEX) {
            require_string(
                scene,
                scene.environment_light.name,
                "environment light name");
        }
        if (scene.environment_light.texture != StaticScene::INVALID_INDEX) {
            require_index(
                scene.environment_light.texture,
                scene.textures.size(),
                "environment light texture");
        }
    }

    void StaticSceneValidator::require_index(
        std::uint64_t index,
        std::uint64_t size,
        std::string_view subject) {

        if (index < size) {
            return;
        }
        log::Logger::g_logger
            << "Invalid StaticScene " << subject
            << " index.\n";
        log::Logger::g_logger.abort();
    }

    void StaticSceneValidator::require_string(
        const StaticScene& scene,
        std::uint32_t offset,
        std::string_view subject) {

        if (offset < scene.strings.size() &&
            std::find(
                scene.strings.begin() + offset,
                scene.strings.end(),
                '\0') != scene.strings.end()) {
            return;
        }
        log::Logger::g_logger
            << "Invalid StaticScene " << subject
            << " string.\n";
        log::Logger::g_logger.abort();
    }

    void StaticSceneValidator::require_range(
        std::uint64_t offset,
        std::uint64_t count,
        std::uint64_t size,
        std::string_view subject,
        std::uint64_t invalid_offset) {

        if (count == 0 && offset == invalid_offset) {
            return;
        }
        if (offset <= size && count <= size - offset) {
            return;
        }
        log::Logger::g_logger
            << "Invalid StaticScene " << subject
            << " range.\n";
        log::Logger::g_logger.abort();
    }

} // namespace fjr::scene
