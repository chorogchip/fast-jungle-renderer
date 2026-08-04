#include "FastJungle/renderer/SceneViewer.hpp"

namespace fjr::render {

	void SceneViewer::init(
		const scene::StaticScene* scene,
		SceneResources* scene_resources) {

		auto& draws = scene_resources->draw_items;
		for (auto& draw : draws) {
			if (draw.instance_kind != SceneResources::InstanceKind::MATRIX)
				continue;
			if (draw.flags != scene::StaticScene::EnumSubmeshFlag::DEFAULT)
				;// continue;

			Draw::DrawDataCpu draw_new{};
			draw_new.constants.offset_instance = draw.constants.instance_offset;
			draw_new.constants.offset_material = draw.constants.material_id;
			draw_new.constants.instnace_kind = draw.constants.instance_kind;
			draw_new.flags = Draw::EnumDrawCpuFlag::DEFAULT;
			draw_new.offset_cbuf_transform = draw.transform_constant_index;
			draw_new.offset_index = draw.first_index;
			draw_new.offset_vertex = draw.base_vertex;
			draw_new.count_index = draw.index_count;
			draw_new.count_instance = draw.instance_count;
			draws_.push_back(draw_new);
		}
		
		scene_raw_ = scene;
		scene_resources_ = scene_resources;
	}

	std::span<const Draw::DrawDataCpu> SceneViewer::get_draw_data() const noexcept {
		return draws_;
	}

}
