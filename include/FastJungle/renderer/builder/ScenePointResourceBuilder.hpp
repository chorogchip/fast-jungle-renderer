#pragma once

#include <span>

#include "FastJungle/renderer/SceneRenderData.hpp"
#include "FastJungle/renderer/component/GPUPointData.hpp"
#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/builder/SceneBoundsBuilder.hpp"

namespace fjr::render {

	class ScenePointResourceBuilder {

	public:
		static ScenePointResources build(
			const scene::StaticScene& scene,
			const SceneBoundsBuilder& bounds,
			std::span<const SceneDrawItem> draw_items);
	};
}