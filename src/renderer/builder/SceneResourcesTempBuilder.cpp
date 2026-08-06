#include "FastJungle/renderer/builder/SceneResourcesTempBuilder.hpp"

#include <cstddef>
#include <cstdint>

#include "FastJungle/renderer/builder/ScenePointResourceBuilder.hpp"

namespace fjr::render {
    data::SceneResourcesTemp SceneResourcesTempBuilder::build(
        const scene::StaticScene& scene,
        const data::SceneBounds& bounds,
        const data::SceneDraws& draws) {
        data::SceneResourcesTemp result;

        // Texture bindings
        result.texture_bindings.reserve(scene.texture_bindings.size() + 1);
        for (const auto& source : scene.texture_bindings) {
            data::StbufTextureBinding binding;
            binding.texture_id = source.texture;
            binding.sampler_id = source.sampler;
            binding.channel = static_cast<std::uint32_t>(source.channel);
            binding.flags = static_cast<std::uint32_t>(source.flags);
            result.texture_bindings.push_back(binding);
        }
        // Default empty binding
        result.texture_bindings.emplace_back();

        // Materials
        result.materials.reserve(scene.materials.size() + 1);
        for (const auto& source : scene.materials) {
            data::StbufMaterial material;
            material.base_color = source.base_color;
            material.emissive_roughness = {
                source.emissive.x,
                source.emissive.y,
                source.emissive.z,
                source.roughness,
            };
            material.surface = {
                source.metallic,
                source.opacity,
                source.opacity_threshold,
                source.ior,
            };
            material.optical = {
                source.specular,
                source.clearcoat,
                source.clearcoat_roughness,
                0.0f,
            };
            material.texture_binding_basecolor = source.texture_binding_base_color;
            material.texture_binding_normal = source.texture_binding_normal;
            material.texture_binding_roughness = source.texture_binding_roughness;
            material.texture_binding_opacity = source.texture_binding_opacity;
            material.texture_binding_emissive = source.texture_binding_emissive;
            material.texture_binding_metallic = source.texture_binding_metallic;
            result.materials.push_back(material);
        }
        // Default material. SceneDrawBuilder는 이 index를
        // scene.materials.size()로 계산한다.
        result.materials.emplace_back();

        // Matrix instances
        result.matrix_instances.reserve(scene.static_mesh_instances.size());
        for (const auto& source : scene.static_mesh_instances) {
            data::StbufMatrixInstance instance;
            instance.transform = source.world_transform;
            result.matrix_instances.push_back(instance);
        }
        // Point draw constants는 PointBatch index와
        // constant index가 일치한다.
        result.point_constants.resize(scene.point_batches.size());
        for (std::size_t batch_id = 0; batch_id < scene.point_batches.size(); ++batch_id) {
            result.point_constants[batch_id].part_local_transform =
                scene.point_batches[batch_id].local_transform;
        }
        // 모든 matrix draw는 완성된 world matrix를
        // instance buffer에서 읽는다.
        if (!scene.static_mesh_instances.empty()) {
            result.matrix_constants.emplace_back();
        }
        result.points = ScenePointResourceBuilder::build(scene, bounds, draws.draw_items);
        return result;
    }
} // namespace fjr::render
