#pragma once

#include <memory>

#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/SceneResources.hpp"

namespace fjr::render {

	class SceneResourcesBuilder {

	public:
		std::unique_ptr<SceneResources> build(const scene::StaticScene& scene);
	};
}