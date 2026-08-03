#include "FastJungle/scene/StaticSceneSerializer.hpp"

#include <vector>
#include <span>
#include <cstring>

namespace fjr::scene {

	namespace {

		template<typename T>
		size_t get_size(const std::vector<T>& v) {
			return v.size() * sizeof(T);
		}

		template<typename T>
		void write(std::byte*& dest, const T& value) {
			std::memcpy(dest, &value, sizeof(T));
			dest += sizeof(T);
		}

		template<typename T>
		void write(std::byte*& dest, const std::vector<T>& value) {
			const size_t size = value.size();
			const size_t data_size = get_size(value);

			write(dest, size);

			if (data_size != 0) {
				std::memcpy(dest, value.data(), data_size);
				dest += data_size;
			}
		}

		template<typename T>
		void read(const std::byte*& src, T& value) {
			std::memcpy(&value, src, sizeof(T));
			src += sizeof(T);
		}

		template<typename T>
		void read(const std::byte*& src, std::vector<T>& value) {
			size_t size;
			read(src, size);

			value.resize(size);

			const size_t data_size = get_size(value);

			if (data_size != 0) {
				std::memcpy(value.data(), src, data_size);
				src += data_size;
			}
		}
	}

	size_t StaticSceneSerializer::calculate_length(const StaticScene& scene) {
		size_t ret = 0;

#define X(type, name) ret += sizeof(size_t) + get_size(scene.name);
		SceneData_MACRO;
#undef X

		ret += sizeof(StaticScene::Camera);
		ret += sizeof(StaticScene::EnvironmentLight);
		ret += sizeof(StaticScene::SceneInfo);

		return ret;
	}

	void StaticSceneSerializer::serialize(std::span<std::byte> dest, const StaticScene& scene) {
		auto ptr = dest.data();

#define X(type, name) write(ptr, scene.name);
		SceneData_MACRO;
#undef X

		write(ptr, scene.camera);
		write(ptr, scene.environment_light);
		write(ptr, scene.info);
	}

	std::unique_ptr<StaticScene> StaticSceneSerializer::deserialize(std::span<const std::byte> src) {
		auto ret = std::make_unique<StaticScene>();
		auto ptr = src.data();

#define X(type, name) read(ptr, ret->name);
		SceneData_MACRO;
#undef X

		read(ptr, ret->camera);
		read(ptr, ret->environment_light);
		read(ptr, ret->info);

		return ret;
	}
}