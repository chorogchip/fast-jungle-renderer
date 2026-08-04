#pragma once

#include <span>
#include <memory>

#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/SceneResources.hpp"
#include "FastJungle/renderer/structs/Draw.hpp"

namespace fjr::render {

	class SceneViewer {

	public:
		void init(
			const scene::StaticScene* scene,
			SceneResources* scene_resources);

		std::span<const Draw::DrawDataCpu> get_draw_data() const;

	private:
		const scene::StaticScene* scene_raw_;
		SceneResources* scene_resources_;
	};
}