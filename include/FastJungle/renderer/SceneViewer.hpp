#pragma once

#include <span>
#include <memory>
#include <vector>

#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/SceneDerivedData.hpp"
#include "FastJungle/renderer/SceneResources.hpp"
#include "FastJungle/renderer/structs/Draw.hpp"

namespace fjr::render {
	class Camera;

	class SceneViewer {

	public:
		void init(
			const scene::StaticScene& scene,
			SceneResources& scene_resources,
			const SceneDerivedData& derived_data);
		void update_visibility(const Camera& camera);

		std::span<const Draw::DrawDataCpu> get_draw_data() const noexcept;

	private:
		struct DrawSource {
			Draw::DrawDataCpu draw;
			math::AABB world_bounds;
		};

		std::vector<DrawSource> draw_sources_;
		std::vector<Draw::DrawDataCpu> draws_;
	};
}
