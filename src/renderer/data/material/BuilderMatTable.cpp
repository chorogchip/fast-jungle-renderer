#include "FastJungle/renderer/data/material/BuilderMatTable.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render::data::mat {

    namespace {

        [[nodiscard]]
        uint32_t get_texture_id(
            const scene::StaticScene& scene,
            uint32_t binding_id) {

            if (binding_id == scene::StaticScene::INVALID_INDEX)
                return Consts::IND_ERR;

            if (binding_id >= scene.texture_bindings.size()) {
                log::Logger::g_logger << log::abrt(
                    "Material contains an invalid texture binding index.");
            }

            const auto texture_id =
                scene.texture_bindings[binding_id].texture;

            if (texture_id == scene::StaticScene::INVALID_INDEX ||
                texture_id >= scene.textures.size()) {

                log::Logger::g_logger << log::abrt(
                    "Texture binding contains an invalid texture index.");
            }

            return texture_id;
        }

        [[nodiscard]]
        uint32_t classify_material_mode(
            const DataPersistent::Material& material,
            bool water,
            std::size_t material_id) {

            const bool has_basecolor =
                material.texture_basecolor != Consts::IND_ERR;
            const bool has_normal =
                material.texture_normal != Consts::IND_ERR;
            const bool has_roughness =
                material.texture_roughness != Consts::IND_ERR;

            if (water) {
                if (!has_basecolor) {
                    log::Logger::g_logger <<
                        "Water material " << material_id << ' ' <<
                        log::abrt("has no base-color texture.");
                }
                return DataPersistent::Material::MODE_WATER;
            }

            if (has_basecolor && has_normal && has_roughness)
                return DataPersistent::Material::MODE_TEXTURED_PBR;

            if (!has_basecolor && !has_normal && !has_roughness)
                return DataPersistent::Material::MODE_CONSTANT_PBR;

            log::Logger::g_logger <<
                "Material " << material_id <<
                " has a partial PBR texture set (base-color=" <<
                has_basecolor << ", normal=" << has_normal <<
                ", roughness=" << has_roughness << "). " <<
                log::abrt(
                    "Runtime material classification requires all or none.");
            return 0;
        }

        void mark_instance_materials(
            std::vector<bool>& destination,
            const scene::StaticScene& scene,
            uint32_t instance_id) {

            const auto& instance = scene.static_mesh_instances[instance_id];
            const auto& mesh = scene.meshes[instance.mesh];

            for (uint32_t lod_id = 0;
                lod_id < mesh.lod_count;
                ++lod_id) {

                const auto& lod = scene.mesh_lods[mesh.lod_offset + lod_id];

                for (uint32_t submesh_id = 0;
                    submesh_id < lod.submesh_count;
                    ++submesh_id) {

                    const auto material = scene.submeshes[
                        lod.submesh_offset + submesh_id].material;
                    destination[material] = true;
                }
            }
        }

        [[nodiscard]]
        std::vector<bool> build_water_material_table(
            const scene::StaticScene& scene) {

            std::vector<bool> result(scene.materials.size());
            mark_instance_materials(
                result,
                scene,
                scene.components.river.instance);
            mark_instance_materials(
                result,
                scene,
                scene.components.creek.instance);

            return result;
        }

        void apply_impostor_parameters(
            std::span<DataPersistent::Material> materials,
            const scene::StaticScene& scene,
            std::span<const DataPersistent::Mesh> meshes) {

            for (const auto& impostor : scene.impostors) {
                const auto center = meshes[impostor.mesh].bounds_center;

                for (uint32_t direction = 0;
                    direction < impostor.direction_count;
                    ++direction) {

                    const auto& card_mesh = scene.meshes[
                        impostor.card_mesh_offset + direction];
                    const auto& card_lod =
                        scene.mesh_lods[card_mesh.lod_offset];
                    const auto& card =
                        scene.submeshes[card_lod.submesh_offset];

                    float half_width = 0.0f;
                    float half_height = 0.0f;

                    for (uint32_t vertex = 0;
                        vertex < card.vertex_count;
                        ++vertex) {

                        const auto& position = scene.vertices[
                            card.vertex_offset + vertex].position;
                        const float x = position.x - center.x;
                        const float y = position.y - center.y;
                        const float z = position.z - center.z;

                        half_width = (std::max)(
                            half_width,
                            std::sqrt(x * x + z * z));
                        half_height = (std::max)(
                            half_height,
                            std::abs(y));
                    }

                    auto& material = materials[card.material];
                    material.flags |=
                        DataPersistent::Material::FLAG_IMPOSTOR;
                    material.impostor_center = center;
                    material.impostor_half_width = half_width;
                    material.impostor_half_height = half_height;
                }
            }
        }

        void apply_source_material(
            DataPersistent::Material& destination,
            const scene::StaticScene& scene,
            const scene::StaticScene::Material& source,
            bool water,
            std::size_t material_id) {

            destination.base_color = {
                source.base_color.x,
                source.base_color.y,
                source.base_color.z,
            };
            destination.roughness = source.roughness;
            destination.opacity_threshold = source.opacity_threshold;
            destination.opacity = source.opacity;
            destination.texture_basecolor = get_texture_id(
                scene,
                source.texture_binding_base_color);
            destination.texture_normal = get_texture_id(
                scene,
                source.texture_binding_normal);
            destination.texture_roughness = get_texture_id(
                scene,
                source.texture_binding_roughness);
            destination.texture_opacity = get_texture_id(
                scene,
                source.texture_binding_opacity);
            destination.flags |= classify_material_mode(
                destination,
                water,
                material_id);
        }

    } // namespace

    std::vector<DataPersistent::Material> BuilderMatTable::build(
        const scene::StaticScene& scene,
        std::span<const DataPersistent::Mesh> meshes) {

        std::vector<DataPersistent::Material> materials(
            scene.materials.size());
        const auto water_materials = build_water_material_table(scene);

        apply_impostor_parameters(materials, scene, meshes);

        for (std::size_t material_id = 0;
            material_id < scene.materials.size();
            ++material_id) {

            apply_source_material(
                materials[material_id],
                scene,
                scene.materials[material_id],
                water_materials[material_id],
                material_id);
        }

        return materials;
    }

} // namespace fjr::render::data::mat
