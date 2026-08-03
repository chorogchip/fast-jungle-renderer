#include "FastJungle/scene/StaticSceneValidation.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fjr::scene {

    namespace {

        [[noreturn]] void fail(
            std::string_view subject,
            std::string_view problem) {

            throw std::runtime_error(
                "Invalid StaticScene " + std::string{subject} + ": " +
                std::string{problem});
        }

        void require_range(
            std::uint64_t offset,
            std::uint64_t count,
            std::size_t size,
            std::string_view subject,
            std::uint64_t invalid_offset = StaticScene::INVALID_INDEX) {

            if (count == 0 && offset == invalid_offset) {
                return;
            }
            if (offset > size || count > size - offset) {
                fail(subject, "range is out of bounds");
            }
        }

        void require_index(
            std::uint32_t index,
            std::size_t size,
            std::string_view subject) {

            if (index >= size) {
                fail(subject, "index is out of bounds");
            }
        }

        void require_optional_index(
            std::uint32_t index,
            std::size_t size,
            std::string_view subject) {

            if (index != StaticScene::INVALID_INDEX) {
                require_index(index, size, subject);
            }
        }

        void require_name(
            const StaticScene& scene,
            std::uint32_t offset,
            std::string_view subject,
            bool optional = false) {

            if (optional && offset == StaticScene::INVALID_INDEX) {
                return;
            }
            if (offset >= scene.strings.size()) {
                fail(subject, "string offset is out of bounds");
            }

            const auto begin = scene.strings.begin() + offset;
            if (std::find(begin, scene.strings.end(), '\0') ==
                scene.strings.end()) {
                fail(subject, "string is not null-terminated");
            }
        }

        template<typename T>
        void require_vector_equal(
            const std::vector<T>& expected,
            const std::vector<T>& actual,
            std::string_view name) {

            if (expected.size() != actual.size()) {
                throw std::runtime_error(
                    "StaticScene round trip changed " + std::string{name} +
                    " length.");
            }
            const std::size_t byte_count = expected.size() * sizeof(T);
            if (byte_count != 0 &&
                std::memcmp(expected.data(), actual.data(), byte_count) != 0) {
                throw std::runtime_error(
                    "StaticScene round trip changed " + std::string{name} +
                    " bytes.");
            }
        }

        template<typename T>
        void require_record_equal(
            const T& expected,
            const T& actual,
            std::string_view name) {

            if (std::memcmp(&expected, &actual, sizeof(T)) != 0) {
                throw std::runtime_error(
                    "StaticScene round trip changed " + std::string{name} +
                    " bytes.");
            }
        }

    } // namespace

    void validate_static_scene(const StaticScene& scene) {
        if (scene.strings.empty() || scene.strings.front() != '\0') {
            fail("strings", "table must begin with an empty string");
        }
        if (scene.info.vertex_count_before_indexing != scene.indices.size()) {
            fail(
                "info.vertex_count_before_indexing",
                "must equal the triangle index count");
        }
        if (scene.info.vertex_count_after_indexing != scene.vertices.size()) {
            fail(
                "info.vertex_count_after_indexing",
                "must equal the indexed vertex count");
        }
        if (scene.info.vertex_count_after_indexing >
            scene.info.vertex_count_before_indexing) {
            fail("info vertex counts", "indexing increased the vertex count");
        }

        for (const auto& texture : scene.textures) {
            require_name(scene, texture.name, "texture name");
            require_range(
                texture.mip_offset,
                texture.mip_count,
                scene.texture_mips.size(),
                "texture mip");
            require_range(
                texture.data_byte_offset,
                texture.data_size,
                scene.texture_data.size(),
                "texture data",
                StaticScene::INVALID_INDEX_64);

            for (std::uint32_t mip_index = 0;
                 mip_index < texture.mip_count;
                 ++mip_index) {
                const auto& mip = scene.texture_mips[
                    texture.mip_offset + mip_index];
                require_range(
                    mip.data_byte_offset_local,
                    mip.slice_pitch,
                    static_cast<std::size_t>(texture.data_size),
                    "texture mip data",
                    StaticScene::INVALID_INDEX_64);
            }
        }

        for (const auto& binding : scene.texture_bindings) {
            require_index(binding.texture, scene.textures.size(),
                "texture binding texture");
            require_index(binding.sampler, scene.samplers.size(),
                "texture binding sampler");
            if (binding.channel > StaticScene::EnumTextureChannel::A) {
                fail("texture binding channel", "enum value is invalid");
            }
            if (binding.flags != StaticScene::EnumTextureBindingFlag::LINEAR &&
                binding.flags != StaticScene::EnumTextureBindingFlag::SRGB) {
                fail("texture binding flags", "enum value is invalid");
            }
        }

        for (const auto& material : scene.materials) {
            require_name(scene, material.name, "material name");
            require_optional_index(material.texture_binding_base_color,
                scene.texture_bindings.size(), "base color binding");
            require_optional_index(material.texture_binding_normal,
                scene.texture_bindings.size(), "normal binding");
            require_optional_index(material.texture_binding_roughness,
                scene.texture_bindings.size(), "roughness binding");
            require_optional_index(material.texture_binding_opacity,
                scene.texture_bindings.size(), "opacity binding");
            require_optional_index(material.texture_binding_emissive,
                scene.texture_bindings.size(), "emissive binding");
        }

        for (const auto& submesh : scene.submeshes) {
            require_name(scene, submesh.name, "submesh name");
            require_range(submesh.vertex_offset, submesh.vertex_count,
                scene.vertices.size(), "submesh vertex");
            require_range(submesh.index_offset, submesh.index_count,
                scene.indices.size(), "submesh index");
            require_index(submesh.material, scene.materials.size(),
                "submesh material");

            for (std::uint32_t index = 0;
                 index < submesh.index_count;
                 ++index) {
                if (scene.indices[submesh.index_offset + index] >=
                    submesh.vertex_count) {
                    fail("submesh index", "value is outside local vertices");
                }
            }
        }

        for (const auto& mesh : scene.meshes) {
            require_name(scene, mesh.name, "mesh name");
            require_range(mesh.submesh_offset, mesh.submesh_count,
                scene.submeshes.size(), "mesh submesh");
        }

        for (const auto& part : scene.prototype_parts) {
            require_index(part.mesh, scene.meshes.size(),
                "prototype part mesh");
        }

        for (const auto& prototype : scene.prototypes) {
            require_name(scene, prototype.name, "prototype name");
            require_range(prototype.part_offset, prototype.part_count,
                scene.prototype_parts.size(), "prototype part");
            if (prototype.object_kind > StaticScene::EnumObjectKind::NETTLE) {
                fail("prototype object kind", "enum value is invalid");
            }
        }

        for (const auto& batch : scene.point_batches) {
            require_name(scene, batch.name, "point batch name");
            require_index(batch.prototype, scene.prototypes.size(),
                "point batch prototype");
            require_range(batch.instance_offset, batch.instance_count,
                scene.point_instances.size(), "point batch instance");
        }

        for (const auto& batch : scene.matrix_batches) {
            require_name(scene, batch.name, "matrix batch name");
            require_index(batch.prototype, scene.prototypes.size(),
                "matrix batch prototype");
            require_range(batch.instance_offset, batch.instance_count,
                scene.matrix_instances.size(), "matrix batch instance");
        }

        require_name(scene, scene.camera.name, "camera name", true);
        require_name(
            scene,
            scene.environment_light.name,
            "environment light name",
            true);
        require_optional_index(
            scene.environment_light.texture,
            scene.textures.size(),
            "environment light texture");
    }

    void require_static_scene_equal(
        const StaticScene& expected,
        const StaticScene& actual) {

#define X(type, name) \
        require_vector_equal(expected.name, actual.name, #name);
        SceneData_MACRO
#undef X

        require_record_equal(expected.camera, actual.camera, "camera");
        require_record_equal(
            expected.environment_light,
            actual.environment_light,
            "environment_light");
        require_record_equal(expected.info, actual.info, "info");
    }

} // namespace fjr::scene
