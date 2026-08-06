#include "FastJungle/renderer/builder/SceneDynamicDataBuilder.hpp"

#include <DirectXMath.h>

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
        bool intersects(
            DirectX::FXMMATRIX view_projection,
            const math::AABB& bounds) noexcept {

            if (!bounds.is_valid()) {
                return true;
            }

            bool outside_left = true;
            bool outside_right = true;
            bool outside_bottom = true;
            bool outside_top = true;
            bool outside_near = true;
            bool outside_far = true;

            for (std::uint32_t corner = 0;
                corner < 8;
                ++corner) {

                const float x =
                    (corner & 1u) != 0
                    ? bounds.max.x
                    : bounds.min.x;

                const float y =
                    (corner & 2u) != 0
                    ? bounds.max.y
                    : bounds.min.y;

                const float z =
                    (corner & 4u) != 0
                    ? bounds.max.z
                    : bounds.min.z;

                DirectX::XMFLOAT4 clip;

                DirectX::XMStoreFloat4(
                    &clip,
                    DirectX::XMVector4Transform(
                        DirectX::XMVectorSet(
                            x,
                            y,
                            z,
                            1.0f),
                        view_projection));

                outside_left &=
                    clip.x < -clip.w;

                outside_right &=
                    clip.x > clip.w;

                outside_bottom &=
                    clip.y < -clip.w;

                outside_top &=
                    clip.y > clip.w;

                outside_near &=
                    clip.z < 0.0f;

                outside_far &=
                    clip.z > clip.w;
            }

            return !(
                outside_left ||
                outside_right ||
                outside_bottom ||
                outside_top ||
                outside_near ||
                outside_far);
        }

        [[nodiscard]]
        float distance_to_bounds(
            const DirectX::XMFLOAT3& point,
            const math::AABB& bounds) noexcept {

            if (!bounds.is_valid()) {
                return 0.0f;
            }

            const auto axis_distance = [](
                float value,
                float minimum,
                float maximum) noexcept {

                    if (value < minimum) {
                        return minimum - value;
                    }

                    if (value > maximum) {
                        return value - maximum;
                    }

                    return 0.0f;
                };

            const float x =
                axis_distance(
                    point.x,
                    bounds.min.x,
                    bounds.max.x);

            const float y =
                axis_distance(
                    point.y,
                    bounds.min.y,
                    bounds.max.y);

            const float z =
                axis_distance(
                    point.z,
                    bounds.min.z,
                    bounds.max.z);

            return std::sqrt(
                x * x +
                y * y +
                z * z);
        }

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

                return std::numeric_limits<
                    float>::infinity();
            }

            return
                object_error *
                world_scale *
                projection_y_scale *
                half_viewport_height /
                distance;
        }

        [[nodiscard]]
        data::DrawFinalCPU make_final_draw(
            const data::DrawFinalGPUIndirect& source) {

            data::DrawFinalCPU result;

            result.constants =
                source.constants;

            result.instnace_class =
                source.instnace_class;

            result.pso_class =
                source.pso_class;

            result.flags =
                source.flags;

            result.offset_cbuf_transform =
                source.offset_cbuf_transform;

            result.offset_index =
                source.offset_index;

            result.offset_vertex =
                source.offset_vertex;

            result.count_index =
                source.count_index;

            result.count_instance =
                source.count_instance;

            return result;
        }

        static constexpr std::uint64_t
            FORCED_LOD0_INDEX_INVOCATION_BUDGET =
            64'000'000;

    } // namespace

    void SceneDynamicDataBuilder::build(
        data::DynamicSceneData& output,
        const data::SceneResourcesTemp& scene,
        const Camera& camera,
        LodSelectionMode lod_selection,
        uint32_t viewport_height) {

        output.visible_draws.clear();

        if (output.visible_draws.capacity() <
            scene.draw_items.size()) {

            output.visible_draws.reserve(
                scene.draw_items.size());
        }

        const bool force_finest =
            lod_selection ==
            LodSelectionMode::FINEST;

        const bool force_coarsest =
            lod_selection ==
            LodSelectionMode::COARSEST;

        std::vector<
            std::pair<
            float,
            const data::DrawFinalGPUIndirect*>>
            forced_lod0_draws;

        if (force_finest) {
            forced_lod0_draws.reserve(
                scene.draw_items.size());
        }

        const auto view_projection =
            DirectX::XMLoadFloat4x4(
                &camera.get_view_projection_mat());

        const float projection_y_scale =
            std::abs(
                camera.get_projection_mat()._22);

        const float half_viewport_height =
            0.5f * static_cast<float>(viewport_height);

        static constexpr float MAX_PIXEL_ERROR =
            1.0f;

        for (const auto& source :
            scene.draw_items) {

            const bool visible =
                intersects(
                    view_projection,
                    source.world_bounds);

            if (!visible) {
                continue;
            }

            const float distance =
                distance_to_bounds(
                    camera.get_position(),
                    source.world_bounds);

            if (force_finest) {

                if (source.lod_error == 0.0f) {
                    forced_lod0_draws.emplace_back(
                        distance,
                        &source);
                }

                continue;
            }

            if (force_coarsest) {

                if (std::isinf(
                    source.next_lod_error)) {

                    output.visible_draws.push_back(
                        make_final_draw(source));
                }

                continue;
            }

            const float current_error =
                projected_error_pixels(
                    source.lod_error,
                    source.world_scale,
                    projection_y_scale,
                    half_viewport_height,
                    distance);

            const float next_error =
                projected_error_pixels(
                    source.next_lod_error,
                    source.world_scale,
                    projection_y_scale,
                    half_viewport_height,
                    distance);

            const bool selected_lod =
                current_error <=
                MAX_PIXEL_ERROR &&
                next_error >
                MAX_PIXEL_ERROR;

            if (selected_lod) {
                output.visible_draws.push_back(
                    make_final_draw(source));
            }
        }

        if (!force_finest) {
            return;
        }

        std::ranges::stable_sort(
            forced_lod0_draws,
            {},
            &std::pair<
            float,
            const data::DrawFinalGPUIndirect*>::
            first);

        std::uint64_t remaining =
            FORCED_LOD0_INDEX_INVOCATION_BUDGET;

        for (const auto& entry :
            forced_lod0_draws) {

            const auto& source =
                *entry.second;

            if (source.count_index == 0 ||
                source.count_instance == 0) {

                continue;
            }

            auto draw =
                make_final_draw(source);

            const std::uint64_t maximum_instances =
                remaining /
                draw.count_index;

            if (maximum_instances == 0) {

                if (!output.visible_draws.empty()) {
                    break;
                }

                draw.count_instance = 1;
            } else {

                draw.count_instance =
                    static_cast<std::uint32_t>(
                        std::min<std::uint64_t>(
                            draw.count_instance,
                            maximum_instances));
            }

            output.visible_draws.push_back(draw);

            const std::uint64_t work =
                static_cast<std::uint64_t>(
                    draw.count_index) *
                draw.count_instance;

            remaining =
                work >= remaining
                ? 0
                : remaining - work;

            if (remaining == 0) {
                break;
            }
        }
    }

} // namespace fjr::render