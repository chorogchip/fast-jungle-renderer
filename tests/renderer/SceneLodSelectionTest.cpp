#include "FastJungle/renderer/component/Camera.hpp"
#include "FastJungle/renderer/builder/SceneViewer.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {

    void require(bool condition, const char* message) {
        if (!condition) {
            std::cerr << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    DirectX::XMFLOAT4X4 camera_at(float z) {
        auto result = fjr::scene::StaticScene::IDENTITY_TRANSFORM;
        result._43 = z;
        return result;
    }

} // namespace

int main() {
    using namespace fjr::render;

    constexpr std::array<float, 4> ERRORS{0.0f, 0.1f, 0.5f, 2.0f};
    std::vector<SceneDrawItem> draw_items;
    for (std::uint32_t lod = 0; lod < ERRORS.size(); ++lod) {
        SceneDrawItem draw;
        draw.instance_kind = SceneResources::InstanceKind::MATRIX;
        draw.index_count = 3;
        draw.first_index = 3 * lod;
        draw.instance_count = 1;
        draw.bounds_index = 0;
        draw.lod_error = ERRORS[lod];
        draw.next_lod_error = lod + 1 < ERRORS.size()
            ? ERRORS[lod + 1]
            : std::numeric_limits<float>::infinity();
        draw_items.push_back(draw);
    }

    SceneBoundsBuilder bounds;
    bounds.static_instance_bounds.push_back({
        -1.0f, -1.0f, 9999.0f,
        1.0f, 1.0f, 10001.0f});
    bounds.static_instance_max_scale.push_back(1.0f);

    SceneViewer viewer;
    viewer.init(draw_items, bounds);

    Camera camera;
    camera.set_viewport(1920, 1080);
    constexpr std::array<float, 4> CAMERA_Z{
        9900.0f, 9500.0f, 8000.0f, 0.0f};
    for (std::uint32_t expected_lod = 0;
         expected_lod < CAMERA_Z.size();
         ++expected_lod) {
        camera.set_world_transform(camera_at(CAMERA_Z[expected_lod]));
        viewer.update_visibility(camera);
        const auto visible = viewer.get_draw_data();
        require(visible.size() == 1, "Exactly one LOD must be visible.");
        require(visible.front().offset_index == 3 * expected_lod,
            "Projected-error LOD selection chose the wrong level.");
    }

    camera.set_world_transform(camera_at(CAMERA_Z.back()));
    viewer.update_visibility(camera, LodSelectionMode::FINEST);
    const auto forced_lod0 = viewer.get_draw_data();
    require(forced_lod0.size() == 1,
        "Forced LOD0 must select exactly one level.");
    require(forced_lod0.front().offset_index == 0,
        "Forced LOD0 selected a simplified level.");

    return EXIT_SUCCESS;
}
