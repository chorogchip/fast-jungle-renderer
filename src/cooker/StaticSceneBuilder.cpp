#pragma once

#include "FastJungle/cooker/StaticSceneBuilder.hpp"

namespace fjr::cooker {

	std::unique_ptr<scene::StaticScene> StaticSceneBuilder::build(const JungleScene& src) {
		auto ret = std::make_unique<scene::StaticScene>();



		return ret;
	}

}