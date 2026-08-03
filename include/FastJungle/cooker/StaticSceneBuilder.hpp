#pragma once

#include <memory>

#include "FastJungle/cooker/JungleScene.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::cooker {

	class StaticSceneBuilder {

	public:
		static std::unique_ptr<scene::StaticScene> build(const JungleScene& src);
	};
}