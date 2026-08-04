#include "FastJungle/renderer/SceneViewer.hpp"

namespace fjr::render {

	void init(
		const scene::StaticScene* scene,
		SceneResources* scene_resources) {

		scene_ = scene;
		scene_resources_ = scene_resources;
	}

	std::span<const Draw::DrawDataCpu> SceneViewer::get_draw_data() const {
		return {};
	}

}