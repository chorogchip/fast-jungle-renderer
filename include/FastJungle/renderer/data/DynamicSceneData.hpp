#pragma once

#include <vector>
#include "FastJungle/renderer/data/RenderTypesDraw.hpp"

namespace fjr::render::data {

	struct DynamicSceneData {

		std::vector<DrawFinalCPU> visible_draws;
	};
}