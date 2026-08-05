#pragma once

#include <span>
#include <memory>
#include <vector>

#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/RendererOptions.hpp"
#include "FastJungle/renderer/builder/SceneBoundsBuilder.hpp"
#include "FastJungle/renderer/SceneRenderData.hpp"
#include "FastJungle/renderer/structs/Draw.hpp"

namespace fjr::render {
	class Camera;

	class SceneViewer {

	public:
		void init(
			std::span<const SceneDrawItem> draw_items,
			const SceneBoundsBuilder& bounds);
		void update_visibility(
			const Camera& camera,
			LodSelectionMode lod_selection =
				LodSelectionMode::AUTOMATIC);

		std::span<const Draw::DrawDataCpu> get_draw_data() const noexcept;

	private:
		struct DrawSource {
			Draw::DrawDataCpu draw;
			math::AABB world_bounds;
			float lod_error = 0.0f;
			float next_lod_error = 0.0f;
			float world_scale = 1.0f;
		};

		std::vector<DrawSource> draw_sources_;
		std::vector<Draw::DrawDataCpu> draws_;
	};
}
