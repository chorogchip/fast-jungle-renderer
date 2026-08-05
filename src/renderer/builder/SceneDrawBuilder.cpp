#include "FastJungle/renderer/builder/SceneDrawBuilder.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace fjr::render {

    namespace {

        struct CompileState {
            SceneRenderData data;
            std::uint32_t default_material_id = 0;
        };

        void append_material_data(
            CompileState& state,
            const scene::StaticScene& scene,
            const RendererOptions& options) {

            auto& bindings = state.data.texture_bindings;
            bindings.reserve(scene.texture_bindings.size() + 1);
            for (const auto& source : scene.texture_bindings) {
                SceneResources::TextureBinding binding;
                binding.texture_id = source.texture;
                binding.sampler_id = source.sampler;
                binding.channel = static_cast<std::uint32_t>(
                    source.channel);
                binding.flags = static_cast<std::uint32_t>(source.flags);
                bindings.push_back(binding);
            }
            bindings.emplace_back();

            auto& materials = state.data.materials;
            materials.reserve(scene.materials.size() + 1);
            for (const auto& source : scene.materials) {
                SceneResources::Material material;
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
                material.texture_bindings_0 = {
                    source.texture_binding_base_color,
                    source.texture_binding_normal,
                    source.texture_binding_roughness,
                    source.texture_binding_opacity,
                };
                material.texture_bindings_1.x =
                    source.texture_binding_emissive;
                material.texture_bindings_1.y =
                    source.texture_binding_metallic;
                materials.push_back(material);
            }

            state.default_material_id =
                static_cast<std::uint32_t>(materials.size());
            materials.emplace_back();
        }

        bool append_mesh_draws(
            CompileState& state,
            const scene::StaticScene& scene,
            std::uint32_t mesh_index,
            SceneResources::InstanceKind instance_kind,
            std::uint32_t instance_offset,
            std::uint32_t instance_count,
            std::uint32_t transform_constant_index,
            std::uint32_t bounds_index) {

            const auto& mesh = scene.meshes[mesh_index];
            bool appended = false;

            for (std::uint32_t local_lod = 0;
                local_lod < mesh.lod_count;
                ++local_lod) {
                const auto& lod = scene.mesh_lods[
                    static_cast<std::size_t>(mesh.lod_offset) + local_lod];
                const float next_lod_error = local_lod + 1 < mesh.lod_count
                    ? scene.mesh_lods[
                        static_cast<std::size_t>(mesh.lod_offset) +
                        local_lod + 1].max_deviation
                    : std::numeric_limits<float>::infinity();

                for (std::uint32_t local_submesh = 0;
                    local_submesh < lod.submesh_count;
                    ++local_submesh) {
                    const auto& submesh = scene.submeshes[
                        static_cast<std::size_t>(lod.submesh_offset) +
                        local_submesh];

                    if (submesh.index_count == 0 || instance_count == 0) {
                        continue;
                    }

                    SceneDrawItem draw;
                    draw.instance_kind = instance_kind;
                    draw.index_count = submesh.index_count;
                    draw.first_index = submesh.index_offset;
                    draw.base_vertex = static_cast<std::int32_t>(
                        submesh.vertex_offset);
                    draw.instance_count = instance_count;
                    draw.constants.instance_offset = instance_offset;
                    draw.constants.material_id =
                        submesh.material == scene::StaticScene::INVALID_INDEX
                        ? state.default_material_id
                        : submesh.material;
                    draw.constants.instance_kind =
                        static_cast<std::uint32_t>(instance_kind);
                    draw.transform_constant_index = transform_constant_index;
                    draw.bounds_index = bounds_index;
                    draw.lod_error = lod.max_deviation;
                    draw.next_lod_error = next_lod_error;
                    draw.flags = submesh.flags;

                    state.data.draw_items.push_back(draw);
                    appended = true;
                }
            }

            return appended;
        }

        void append_point_range(
            CompileState& state,
            const scene::StaticScene& scene,
            scene::StaticScene::IndexRange range) {

            for (std::uint32_t local_batch = 0;
                local_batch < range.count;
                ++local_batch) {
                const auto batch_index = range.offset + local_batch;
                const auto& batch = scene.point_batches[batch_index];
                if (batch.instance_count == 0) {
                    continue;
                }

                const auto& definition =
                    scene.instanced_mesh_definitions[batch.definition];
                const auto constant_index = static_cast<std::uint32_t>(
                    state.data.point_draw_constants.size());

                const bool appended = append_mesh_draws(
                    state,
                    scene,
                    definition.mesh,
                    SceneResources::InstanceKind::POINT,
                    batch.instance_offset,
                    batch.instance_count,
                    constant_index,
                    batch_index);

                if (appended) {
                    SceneResources::PointDrawConstants constants;
                    constants.part_local_transform =
                        definition.local_transform;
                    constants.batch_local_to_world = batch.local_to_world;
                    state.data.point_draw_constants.push_back(constants);
                }
            }
        }

        void append_point_draws(
            CompileState& state,
            const scene::StaticScene& scene,
            const RendererOptions& options) {

            if (options.object_selection == ObjectSelectionMode::DEMO_PYRAMID ||
                options.object_selection == ObjectSelectionMode::DEMO_BASIC)
                return;

            const auto& components = scene.components;
            append_point_range(state, scene,
                components.anthurium.point_batches);
            append_point_range(state, scene,
                components.nettle.point_batches);
            append_point_range(state, scene,
                components.shrub_sorrel.point_batches);
            append_point_range(state, scene,
                components.shrub.point_batches);
            append_point_range(state, scene,
                components.grass_b.point_batches);
            append_point_range(state, scene,
                components.grass_a.point_batches);
            append_point_range(state, scene,
                components.pyramid_grass_b.point_batches);
            append_point_range(state, scene,
                components.pyramid_moss.point_batches);
            append_point_range(state, scene,
                components.queen_forest.point_batches);
            append_point_range(state, scene,
                components.river_forest.point_batches);
            append_point_range(state, scene,
                components.river_sapling.point_batches);
            append_point_range(state, scene,
                components.river_seedling.point_batches);
        }

        void append_static_instance(
            CompileState& state,
            const scene::StaticScene& scene,
            std::uint32_t instance_index) {

            if (instance_index == scene::StaticScene::INVALID_INDEX)
                return;

            const auto& instance =
                scene.static_mesh_instances[instance_index];
            (void)append_mesh_draws(
                state,
                scene,
                instance.mesh,
                SceneResources::InstanceKind::MATRIX,
                instance_index,
                1,
                0,
                instance_index);
        }

        void append_static_range(
            CompileState& state,
            const scene::StaticScene& scene,
            scene::StaticScene::IndexRange range) {

            for (std::uint32_t local_instance = 0;
                local_instance < range.count;
                ++local_instance) {
                append_static_instance(
                    state,
                    scene,
                    range.offset + local_instance);
            }
        }

        void append_static_draws(
            CompileState& state,
            const scene::StaticScene& scene,
            const RendererOptions& options) {

            auto& matrix_instances = state.data.matrix_instances;
            matrix_instances.reserve(scene.static_mesh_instances.size());
            for (const auto& source : scene.static_mesh_instances) {
                SceneResources::MatrixInstance destination;
                destination.transform = source.world_transform;
                matrix_instances.push_back(destination);
            }

            if (!scene.static_mesh_instances.empty()) {
                // Static instance matrices already contain their complete
                // world transform, so all draws share the identity constant.
                state.data.matrix_draw_constants.emplace_back();
            }

            const auto& components = scene.components;
            append_static_instance(state, scene,
                components.pyramid.instance);
            append_static_instance(state, scene,
                components.river.instance);
            append_static_instance(state, scene,
                components.creek.instance);
            append_static_instance(state, scene,
                components.banyan.instance);
            append_static_range(state, scene,
                components.terrain.extended);
            append_static_range(state, scene,
                components.terrain.cinematic);
        }

    } // namespace

    SceneRenderData SceneDrawBuilder::build(
        const scene::StaticScene& scene,
        const RendererOptions& options) {

        CompileState state;
        append_material_data(state, scene, options);
        append_point_draws(state, scene, options);
        append_static_draws(state, scene, options);
        return std::move(state.data);
    }

} // namespace fjr::render
