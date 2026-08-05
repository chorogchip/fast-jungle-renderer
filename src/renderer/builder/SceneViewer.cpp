#include "FastJungle/renderer/builder/SceneViewer.hpp"
#include "FastJungle/renderer/component/Camera.hpp"

#include <cmath>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace fjr::render {
	namespace {

		[[nodiscard]] bool intersects(
			DirectX::FXMMATRIX view_projection,
			const math::AABB& bounds) noexcept {

			if (!bounds.is_valid() ||
				!std::isfinite(bounds.min.x) ||
				!std::isfinite(bounds.min.y) ||
				!std::isfinite(bounds.min.z) ||
				!std::isfinite(bounds.max.x) ||
				!std::isfinite(bounds.max.y) ||
				!std::isfinite(bounds.max.z)) {
				return true;
			}

			bool outside_left = true;
			bool outside_right = true;
			bool outside_bottom = true;
			bool outside_top = true;
			bool outside_near = true;
			bool outside_far = true;

			for (std::uint32_t corner = 0; corner < 8; ++corner) {
				const float x = (corner & 1u) != 0
					? bounds.max.x
					: bounds.min.x;
				const float y = (corner & 2u) != 0
					? bounds.max.y
					: bounds.min.y;
				const float z = (corner & 4u) != 0
					? bounds.max.z
					: bounds.min.z;
				DirectX::XMFLOAT4 clip;
				DirectX::XMStoreFloat4(
					&clip,
					DirectX::XMVector4Transform(
						DirectX::XMVectorSet(x, y, z, 1.0f),
						view_projection));

				outside_left &= clip.x < -clip.w;
				outside_right &= clip.x > clip.w;
				outside_bottom &= clip.y < -clip.w;
				outside_top &= clip.y > clip.w;
				outside_near &= clip.z < 0.0f;
				outside_far &= clip.z > clip.w;
			}

			return !(
				outside_left ||
				outside_right ||
				outside_bottom ||
				outside_top ||
				outside_near ||
				outside_far);
		}

		[[nodiscard]] float distance_to_bounds(
			const DirectX::XMFLOAT3& point,
			const math::AABB& bounds) noexcept {

			if (!bounds.is_valid()) {
				return 0.0f;
			}
			const auto axis_distance = [](float value, float minimum, float maximum) {
				return value < minimum
					? minimum - value
					: (value > maximum ? value - maximum : 0.0f);
			};
			const float x = axis_distance(point.x, bounds.min.x, bounds.max.x);
			const float y = axis_distance(point.y, bounds.min.y, bounds.max.y);
			const float z = axis_distance(point.z, bounds.min.z, bounds.max.z);
			return std::sqrt(x * x + y * y + z * z);
		}

		[[nodiscard]] float projected_error_pixels(
			float object_error,
			float world_scale,
			float projection_y_scale,
			float half_viewport_height,
			float distance) noexcept {

			if (object_error == 0.0f || world_scale == 0.0f) {
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

		// Full Jungle LOD0 contains tens of millions of triangles multiplied
		// across millions of instances. A forced preview must bound one frame's
		// vertex work or Windows will reset the GPU before the camera can move.
		constexpr std::uint64_t FORCED_LOD0_INDEX_INVOCATION_BUDGET =
			64'000'000;

	} // namespace

	void SceneViewer::init(
		std::span<const SceneDrawItem> draw_items,
		const SceneBoundsBuilder& bounds) {

		draw_sources_.clear();
		draws_.clear();

		for (const auto& draw : draw_items) {
			if (draw.index_count == 0 || draw.instance_count == 0)
				continue;

			const bool point =
				draw.instance_kind == SceneResources::InstanceKind::POINT;
			const auto bounds_count = point
				? bounds.point_batch_bounds.size()
				: bounds.static_instance_bounds.size();
			if (draw.bounds_index >= bounds_count) {
				throw std::logic_error(
					"Scene draw has no matching bounds.");
			}
			const auto& world_bounds = point
				? bounds.point_batch_bounds[draw.bounds_index]
				: bounds.static_instance_bounds[draw.bounds_index];
			const float world_scale = point
				? bounds.point_batch_max_scale[draw.bounds_index]
				: bounds.static_instance_max_scale[draw.bounds_index];

			Draw::DrawDataCpu draw_new{};
			draw_new.constants.offset_instance = draw.constants.instance_offset;
			draw_new.constants.offset_material = draw.constants.material_id;
			draw_new.constants.instnace_kind = draw.constants.instance_kind;

			std::uint32_t pipeline_flags = 0;
			const auto submesh_flags =
				static_cast<std::uint32_t>(draw.flags);
			if ((submesh_flags & static_cast<std::uint32_t>(
				scene::StaticScene::EnumSubmeshFlag::DOUBLE_SIDED)) != 0) {
				pipeline_flags |= static_cast<std::uint32_t>(
					Draw::EnumDrawCpuFlag::DOUBLE_SIDED);
			}
			if ((submesh_flags & static_cast<std::uint32_t>(
				scene::StaticScene::EnumSubmeshFlag::ALPHA_BLENDED)) != 0) {
				pipeline_flags |= static_cast<std::uint32_t>(
					Draw::EnumDrawCpuFlag::ALPHA_BLENDED);
			}
			draw_new.flags = static_cast<Draw::EnumDrawCpuFlag>(
				pipeline_flags);
			draw_new.offset_cbuf_transform = draw.transform_constant_index;
			draw_new.offset_index = draw.first_index;
			draw_new.offset_vertex = draw.base_vertex;
			draw_new.count_index = draw.index_count;
			draw_new.count_instance = draw.instance_count;

			DrawSource source;
			source.draw = draw_new;
			source.world_bounds = world_bounds;
			source.lod_error = draw.lod_error;
			source.next_lod_error = draw.next_lod_error;
			source.world_scale = world_scale;
			draw_sources_.push_back(source);
		}

		draws_.reserve(draw_sources_.size());
	}

	void SceneViewer::update_visibility(
		const Camera& camera,
		LodSelectionMode lod_selection) {
		draws_.clear();
		const bool force_finest =
			lod_selection == LodSelectionMode::FINEST;
		const bool force_coarsest =
			lod_selection == LodSelectionMode::COARSEST;
		std::vector<std::pair<float, const DrawSource*>> forced_lod0_draws;
		if (force_finest) {
			forced_lod0_draws.reserve(draw_sources_.size());
		}

		const auto view_projection = DirectX::XMLoadFloat4x4(
			&camera.get_view_projection());
		const float projection_y_scale = std::abs(
			camera.get_projection()._22);
		const float half_viewport_height =
			0.5f * static_cast<float>(camera.get_viewport_height());
		constexpr float MAX_PIXEL_ERROR = 1.0f;

		for (const auto& source : draw_sources_) {
			const float distance = distance_to_bounds(
				camera.get_world_position(), source.world_bounds);
			const float current_error = projected_error_pixels(
				source.lod_error,
				source.world_scale,
				projection_y_scale,
				half_viewport_height,
				distance);
			const float next_error = projected_error_pixels(
				source.next_lod_error,
				source.world_scale,
				projection_y_scale,
				half_viewport_height,
				distance);
			const bool visible = intersects(
				view_projection,
				source.world_bounds);
			if (force_finest) {
				if (source.lod_error == 0.0f && visible) {
					forced_lod0_draws.emplace_back(distance, &source);
				}
				continue;
			}
			if (force_coarsest) {
				if (std::isinf(source.next_lod_error) && visible) {
					draws_.push_back(source.draw);
				}
				continue;
			}

			const bool selected_lod =
				current_error <= MAX_PIXEL_ERROR &&
				next_error > MAX_PIXEL_ERROR;
			if (selected_lod && visible) {
				draws_.push_back(source.draw);
			}
		}

		if (!force_finest) {
			return;
		}

		std::ranges::stable_sort(
			forced_lod0_draws,
			{},
			&std::pair<float, const DrawSource*>::first);
		std::uint64_t remaining = FORCED_LOD0_INDEX_INVOCATION_BUDGET;
		for (const auto& [distance, source] : forced_lod0_draws) {
			(void)distance;
			if (source->draw.count_index == 0 ||
				source->draw.count_instance == 0) {
				continue;
			}

			auto draw = source->draw;
			const std::uint64_t maximum_instances =
				remaining / draw.count_index;
			if (maximum_instances == 0) {
				if (!draws_.empty()) {
					break;
				}
				draw.count_instance = 1;
			} else {
				draw.count_instance = static_cast<std::uint32_t>(
					std::min<std::uint64_t>(
						draw.count_instance,
						maximum_instances));
			}
			draws_.push_back(draw);

			const std::uint64_t work =
				static_cast<std::uint64_t>(draw.count_index) *
				draw.count_instance;
			remaining = work >= remaining ? 0 : remaining - work;
			if (remaining == 0) {
				break;
			}
		}
	}

	std::span<const Draw::DrawDataCpu> SceneViewer::get_draw_data() const noexcept {
		return draws_;
	}

}
