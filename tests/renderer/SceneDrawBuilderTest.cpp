#include "FastJungle/renderer/builder/SceneDrawBuilder.hpp"

#include <cmath>
#include <stdexcept>

namespace {

    void require(bool condition, const char* message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

} // namespace

int main() {
    using fjr::render::SceneDrawBuilder;
    using fjr::render::SceneResources;
    using fjr::scene::StaticScene;

    StaticScene scene;
    scene.materials.emplace_back();

    StaticScene::Submesh lod0_submesh;
    lod0_submesh.vertex_offset = 4;
    lod0_submesh.index_offset = 12;
    lod0_submesh.index_count = 6;
    lod0_submesh.material = StaticScene::INVALID_INDEX;
    lod0_submesh.flags = StaticScene::EnumSubmeshFlag::ALPHA_BLENDED;
    scene.submeshes.push_back(lod0_submesh);

    StaticScene::Submesh lod1_submesh;
    lod1_submesh.vertex_offset = 10;
    lod1_submesh.index_offset = 18;
    lod1_submesh.index_count = 3;
    lod1_submesh.material = 0;
    scene.submeshes.push_back(lod1_submesh);

    scene.mesh_lods.push_back({
        .submesh_offset = 0,
        .submesh_count = 1,
        .max_deviation = 0.0f,
    });
    scene.mesh_lods.push_back({
        .submesh_offset = 1,
        .submesh_count = 1,
        .max_deviation = 0.25f,
    });
    scene.meshes.push_back({
        .lod_offset = 0,
        .lod_count = 2,
    });

    StaticScene::StaticMeshInstance instance;
    instance.mesh = 0;
    scene.static_mesh_instances.push_back(instance);
    scene.components.pyramid.instance = 0;

    const auto data = SceneDrawBuilder::build(scene);

    require(data.materials.size() == 2,
        "Builder did not append the default material.");
    require(data.texture_bindings.size() == 1,
        "Builder did not append the default texture binding.");
    require(data.matrix_instances.size() == 1,
        "Builder did not stage the static instance matrix.");
    require(data.matrix_draw_constants.size() == 1,
        "Builder did not create the shared matrix draw constant.");
    require(data.draw_items.size() == 2,
        "Builder did not preserve both mesh LOD draws.");

    const auto& lod0 = data.draw_items[0];
    const auto& lod1 = data.draw_items[1];
    require(lod0.instance_kind == SceneResources::InstanceKind::MATRIX,
        "Static draw instance kind is invalid.");
    require(lod0.constants.material_id == 1,
        "Invalid material did not resolve to the default material.");
    require(lod0.first_index == 12 && lod0.index_count == 6,
        "LOD0 geometry range changed during compilation.");
    require(lod0.lod_error == 0.0f && lod0.next_lod_error == 0.25f,
        "LOD0 error interval is invalid.");
    require(lod0.flags == StaticScene::EnumSubmeshFlag::ALPHA_BLENDED,
        "Submesh flags changed during compilation.");

    require(lod1.constants.material_id == 0,
        "Explicit material index changed during compilation.");
    require(lod1.first_index == 18 && lod1.index_count == 3,
        "LOD1 geometry range changed during compilation.");
    require(lod1.lod_error == 0.25f &&
        std::isinf(lod1.next_lod_error),
        "Final LOD error interval is invalid.");

    return 0;
}
