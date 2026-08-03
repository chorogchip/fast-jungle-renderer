#pragma once

#include <memory>
#include <span>
#include <cstddef>

#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::scene {

	class StaticSceneSerializer {

	public:
		static size_t calculate_length(const StaticScene& scene);
		static void serialize(std::span<std::byte> dest, const StaticScene& scene);
		static std::unique_ptr<StaticScene> deserialize(std::span<const std::byte> src);
	};
}