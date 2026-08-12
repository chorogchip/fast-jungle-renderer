#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/data/DataPersistent.hpp"

namespace fjr::render::data::geom {

	class BuilderGeomSubmesh {

	public:
		static std::vector<DataPersistent::SubMesh> build(
			const scene::StaticScene& scene,
            std::span<const std::int32_t> base_vertices);
	};
}
