#include "FastJungle/renderer/builder/SceneDynamicDataBuilder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <ranges>
#include <utility>
#include <vector>

namespace fjr::render {

    namespace {

        [[nodiscard]]
        float projected_error_pixels(
            float object_error,
            float world_scale,
            float projection_y_scale,
            float half_viewport_height,
            float distance) noexcept {
            if (object_error == 0.0f ||
                world_scale == 0.0f) {
                return 0.0f;
            }

            if (!std::isfinite(object_error) ||
                !std::isfinite(world_scale) ||
                world_scale < 0.0f ||
                distance <= 0.0f) {
                return std::numeric_limits<float>::infinity();
            }
            return object_error * world_scale * projection_y_scale *
                half_viewport_height / distance;
        }

        [[nodiscard]]
        data::DrawFinalCPU make_final_draw(
            const data::DrawFinalGPUIndirect& source) {
            data::DrawFinalCPU result;
            result.constants = source.constants;
            result.instnace_class = source.instnace_class;
            result.pso_class = source.pso_class;
            result.flags = source.flags;
            result.offset_cbuf_transform = source.offset_cbuf_transform;
            result.offset_index = source.offset_index;
            result.offset_vertex = source.offset_vertex;
            result.count_index = source.count_index;
            result.count_instance = source.count_instance;
            return result;
        }

    } // namespace

    void SceneDynamicDataBuilder::build(
        data::DynamicSceneData& output,
        const data::SceneDraws& scene,
        const Camera& camera,
        LodSelectionMode lod_selection,
        uint32_t viewport_height) {
        output.visible_draws.clear();
        if (output.visible_draws.capacity() < scene.draw_items.size()) {
            output.visible_draws.reserve(scene.draw_items.size());
        }
        const bool force_finest = lod_selection == LodSelectionMode::FINEST;
        const bool force_coarsest = lod_selection == LodSelectionMode::COARSEST;
        std::vector<std::pair<float, const data::DrawFinalGPUIndirect*>> forced_lod0_draws;
        if (force_finest) {
            forced_lod0_draws.reserve(scene.draw_items.size());
        }
        const auto frustum = camera.make_frustum();
        float projection_y_scale = 0.0f;
        float half_viewport_height = 0.0f;
        if (!force_finest && !force_coarsest) {
            projection_y_scale = std::abs(camera.get_projection_mat()._22);
            half_viewport_height = 0.5f * static_cast<float>(viewport_height);
        }
        static constexpr float MAX_PIXEL_ERROR = 1.0f;

        for (const auto& source : scene.draw_items) {
            const bool visible = frustum.intersects(source.world_bounds);
            if (!visible) {
                continue;
            }

            if (force_coarsest) {
                if (std::isinf(source.next_lod_error)) {
                    output.visible_draws.push_back(make_final_draw(source));
                }
                continue;
            }
            const float distance = source.world_bounds.distance_to(camera.get_position());

            if (force_finest) {
                if (source.lod_error == 0.0f) {
                    forced_lod0_draws.emplace_back(distance, &source);
                }
                continue;
            }
            const float current_error = projected_error_pixels(
                source.lod_error, source.world_scale, projection_y_scale,
                half_viewport_height, distance);
            const float next_error = projected_error_pixels(
                source.next_lod_error, source.world_scale, projection_y_scale,
                half_viewport_height, distance);
            const bool selected_lod =
                current_error <= MAX_PIXEL_ERROR && next_error > MAX_PIXEL_ERROR;

            if (selected_lod) {
                output.visible_draws.push_back(make_final_draw(source));
            }
        }
        if (!force_finest) {
            return;
        }
        std::ranges::stable_sort(
            forced_lod0_draws,
            {},
            &std::pair<float, const data::DrawFinalGPUIndirect*>::first);

        for (const auto& entry : forced_lod0_draws) {
            const auto& source = *entry.second;
            if (source.count_index == 0 ||
                source.count_instance == 0) {
                continue;
            }
            output.visible_draws.push_back(make_final_draw(source));
        }
    }

} // namespace fjr::render
