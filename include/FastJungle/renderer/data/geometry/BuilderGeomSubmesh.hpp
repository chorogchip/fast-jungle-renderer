#pragma once

#include <vector>
#include <DirectXMath.h>

#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/data/DataPersistent.hpp"

namespace fjr::render::data::geom {

	class BuilderGeomSubmesh {

	public:
		static std::vector<DataPersistent::SubMesh> build(
			const scene::StaticScene& scene);
	};
}