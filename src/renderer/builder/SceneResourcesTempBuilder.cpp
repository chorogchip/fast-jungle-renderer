#include "FastJungle/renderer/builder/SceneResourcesTempBuilder.hpp"

#include <cstddef>
#include <cstdint>

#include "FastJungle/renderer/builder/SceneDrawBuilder.hpp"
#include "FastJungle/renderer/builder/ScenePointResourceBuilder.hpp"

namespace fjr::render {

    data::SceneResourcesTemp
        SceneResourcesTempBuilder::build(
            const scene::StaticScene& scene,
            const data::SceneBounds& bounds,
            const RendererOptions& options) {

        data::SceneResourcesTemp result;

        // Texture bindings
        result.texture_bindings.reserve(
            scene.texture_bindings.size() + 1);

        for (const auto& source :
            scene.texture_bindings) {

            data::StbufTextureBinding output;

            output.texture_id =
                source.texture;

            output.sampler_id =
                source.sampler;

            output.channel =
                static_cast<std::uint32_t>(
                    source.channel);

            output.flags =
                static_cast<std::uint32_t>(
                    source.flags);

            result.texture_bindings.push_back(
                output);
        }

        // Default empty binding
        result.texture_bindings.emplace_back();

        // Materials
        result.materials.reserve(
            scene.materials.size() + 1);

        for (const auto& source :
            scene.materials) {

            data::StbufMaterial output;

            output.base_color =
                source.base_color;

            output.emissive_roughness = {
                source.emissive.x,
                source.emissive.y,
                source.emissive.z,
                source.roughness
            };

            output.surface = {
                source.metallic,
                source.opacity,
                source.opacity_threshold,
                source.ior
            };

            output.optical = {
                source.specular,
                source.clearcoat,
                source.clearcoat_roughness,
                0.0f
            };

            output.texture_binding_basecolor =
                source.texture_binding_base_color;

            output.texture_binding_normal =
                source.texture_binding_normal;

            output.texture_binding_roughness =
                source.texture_binding_roughness;

            output.texture_binding_opacity =
                source.texture_binding_opacity;

            output.texture_binding_emissive =
                source.texture_binding_emissive;

            output.texture_binding_metallic =
                source.texture_binding_metallic;

            result.materials.push_back(output);
        }

        // Default material. SceneDrawBuilder는 이 index를
        // scene.materials.size()로 계산한다.
        result.materials.emplace_back();

        // Matrix instances
        result.matrix_instances.reserve(
            scene.static_mesh_instances.size());

        for (const auto& source :
            scene.static_mesh_instances) {

            data::StbufMatrixInstance output;
            output.transform =
                source.world_transform;

            result.matrix_instances.push_back(
                output);
        }

        // Point draw constants는 PointMeshBatch index와
        // constant index가 일치한다.
        result.point_constants.resize(
            scene.point_mesh_batches.size());

        for (std::size_t batch_index = 0;
            batch_index < scene.point_mesh_batches.size();
            ++batch_index) {

            const auto& batch =
                scene.point_mesh_batches[batch_index];

            auto& output =
                result.point_constants[batch_index];

            output.part_local_transform =
                batch.local_transform;
        }

        // 모든 matrix draw는 완성된 world matrix를
        // instance buffer에서 읽는다.
        if (!scene.static_mesh_instances.empty()) {
            result.matrix_constants.emplace_back();
        }

        result.draw_items =
            SceneDrawBuilder::build(
                scene,
                bounds,
                options);

        result.points =
            ScenePointResourceBuilder::build(
                scene,
                bounds,
                result.draw_items);

        result.world_bounds =
            bounds.world_bounds;

        return result;
    }

} // namespace fjr::render
