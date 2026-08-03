#include "FastJungle/scene/StaticSceneSerializer.hpp"

#include <vector>

namespace fjr::scene {

	namespace {

		template<typename T>
		size_t get_size(const std::vector<T>& v) {
			return v.size() * sizeof(T);
		}
	}

	size_t StaticSceneSerializer::calculate_length(const StaticScene& scene) {
		size_t ret = 0;

		ret += 0;

		return ret;
	}

	void StaticSceneSerializer::serialize(std::span<std::byte> dest, const StaticScene& scene) {

	}

	std::unique_ptr<StaticScene> StaticSceneSerializer::deserialize(std::span<const std::byte> src) {
		auto ret = std::make_unique<StaticScene>();

		return ret;
	}
}