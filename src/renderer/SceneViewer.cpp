#include "FastJungle/renderer/SceneViewer.hpp"
#include "FastJungle/renderer/Camera.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

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

	} // namespace

	void SceneViewer::init(
		const scene::StaticScene& scene,
		SceneResources& scene_resources,
		const SceneDerivedData& derived_data) {

		draw_sources_.clear();
		draws_.clear();

		std::unordered_map<std::uint32_t, math::AABB>
			point_bounds_by_offset;
		point_bounds_by_offset.reserve(scene.point_batches.size());
		for (std::size_t index = 0;
			 index < scene.point_batches.size();
			 ++index) {
			const auto& batch = scene.point_batches[index];
			point_bounds_by_offset.emplace(
				batch.instance_offset,
				derived_data.point_batch_bounds[index]);
		}

		std::unordered_map<std::uint32_t, math::AABB>
			matrix_bounds_by_offset;
		matrix_bounds_by_offset.reserve(scene.matrix_batches.size());
		for (std::size_t index = 0;
			 index < scene.matrix_batches.size();
			 ++index) {
			const auto& batch = scene.matrix_batches[index];
			matrix_bounds_by_offset.emplace(
				batch.instance_offset,
				derived_data.matrix_batch_bounds[index]);
		}

		auto& draws = scene_resources.draw_items;
		for (auto& draw : draws) {
			if (draw.index_count == 0 || draw.instance_count == 0)
				continue;

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

			const auto& bounds_by_offset =
				draw.instance_kind == SceneResources::InstanceKind::POINT
				? point_bounds_by_offset
				: matrix_bounds_by_offset;
			const auto bounds = bounds_by_offset.find(
				draw.constants.instance_offset);

			DrawSource source;
			source.draw = draw_new;
			if (bounds != bounds_by_offset.end()) {
				source.world_bounds = bounds->second;
			}
			draw_sources_.push_back(source);
		}

		draws_.reserve(draw_sources_.size());
	}

	void SceneViewer::update_visibility(const Camera& camera) {
		draws_.clear();

		const auto view_projection = DirectX::XMLoadFloat4x4(
			&camera.get_view_projection());

		for (const auto& source : draw_sources_) {
			if (intersects(view_projection, source.world_bounds)) {
				draws_.push_back(source.draw);
			}
		}
	}

	std::span<const Draw::DrawDataCpu> SceneViewer::get_draw_data() const noexcept {
		return draws_;
	}

}
