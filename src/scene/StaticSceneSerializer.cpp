#include "FastJungle/scene/StaticSceneSerializer.hpp"

#include <array>
#include <limits>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace fjr::scene {

	namespace {
		constexpr std::array<char, 8> FILE_MAGIC{
			'F', 'J', 'S', 'C', 'E', 'N', 'E', '\0'};
		constexpr std::uint32_t FILE_VERSION = 1;

		struct FileHeader {
			std::array<char, 8> magic = FILE_MAGIC;
			std::uint32_t version = FILE_VERSION;
			std::uint32_t header_size = 32;
			std::uint32_t vertex_size = sizeof(StaticScene::Vertex);
			std::uint32_t scene_info_size = sizeof(StaticScene::SceneInfo);
			std::uint64_t payload_size = 0;
		};

		static_assert(sizeof(FileHeader) == 32);

		template<typename T>
		size_t get_size(const std::vector<T>& value) {
			if (value.size() >
				std::numeric_limits<size_t>::max() / sizeof(T)) {
				throw std::length_error("StaticScene vector is too large.");
			}
			return value.size() * sizeof(T);
		}

		void checked_add(size_t& total, size_t amount) {
			if (amount > std::numeric_limits<size_t>::max() - total) {
				throw std::length_error("StaticScene serialization is too large.");
			}
			total += amount;
		}

		class Writer {
		public:
			explicit Writer(std::span<std::byte> destination)
				: destination_(destination) {}

			template<typename T>
			void write(const T& value) {
				write_bytes(&value, sizeof(T));
			}

			template<typename T>
			void write(const std::vector<T>& value) {
				const size_t size = value.size();
				write(size);
				write_bytes(value.data(), get_size(value));
			}

			[[nodiscard]] size_t offset() const noexcept { return offset_; }

		private:
			void write_bytes(const void* source, size_t length) {
				if (length > destination_.size() - offset_) {
					throw std::runtime_error(
						"StaticScene serialization destination is too small.");
				}
				if (length != 0) {
					std::memcpy(destination_.data() + offset_, source, length);
					offset_ += length;
				}
			}

			std::span<std::byte> destination_;
			size_t offset_ = 0;
		};

		class Reader {
		public:
			explicit Reader(std::span<const std::byte> source)
				: source_(source) {}

			template<typename T>
			void read(T& value) {
				read_bytes(&value, sizeof(T));
			}

			template<typename T>
			void read(std::vector<T>& value) {
				size_t size = 0;
				read(size);
				if (size > remaining() / sizeof(T)) {
					throw std::runtime_error(
						"StaticScene vector exceeds the input payload.");
				}
				value.resize(size);
				read_bytes(value.data(), get_size(value));
			}

			[[nodiscard]] size_t remaining() const noexcept {
				return source_.size() - offset_;
			}

		private:
			void read_bytes(void* destination, size_t length) {
				if (length > remaining()) {
					throw std::runtime_error(
						"StaticScene input is truncated.");
				}
				if (length != 0) {
					std::memcpy(destination, source_.data() + offset_, length);
					offset_ += length;
				}
			}

			std::span<const std::byte> source_;
			size_t offset_ = 0;
		};
	}

	size_t StaticSceneSerializer::calculate_length(const StaticScene& scene) {
		size_t ret = sizeof(FileHeader);

#define X(type, name) \
		checked_add(ret, sizeof(size_t)); \
		checked_add(ret, get_size(scene.name));
		SceneData_MACRO;
#undef X

		checked_add(ret, sizeof(StaticScene::Camera));
		checked_add(ret, sizeof(StaticScene::EnvironmentLight));
		checked_add(ret, sizeof(StaticScene::SceneInfo));

		return ret;
	}

	void StaticSceneSerializer::serialize(std::span<std::byte> dest, const StaticScene& scene) {
		const size_t expected_length = calculate_length(scene);
		if (dest.size() != expected_length) {
			throw std::invalid_argument(
				"StaticScene serialization destination has the wrong length.");
		}
		Writer writer{dest};
		FileHeader header{};
		header.payload_size = dest.size() - sizeof(FileHeader);
		writer.write(header);

#define X(type, name) writer.write(scene.name);
		SceneData_MACRO;
#undef X

		writer.write(scene.camera);
		writer.write(scene.environment_light);
		writer.write(scene.info);
		if (writer.offset() != dest.size()) {
			throw std::runtime_error(
				"StaticScene serialization length changed unexpectedly.");
		}
	}

	std::unique_ptr<StaticScene> StaticSceneSerializer::deserialize(std::span<const std::byte> src) {
		auto ret = std::make_unique<StaticScene>();
		Reader reader{src};
		FileHeader header{};
		reader.read(header);
		if (header.magic != FILE_MAGIC) {
			throw std::runtime_error(
				"StaticScene file magic is invalid or the file uses the "
				"unversioned tangent vertex format.");
		}
		if (header.version != FILE_VERSION) {
			throw std::runtime_error("StaticScene file version is unsupported.");
		}
		if (header.header_size != sizeof(FileHeader) ||
			header.vertex_size != sizeof(StaticScene::Vertex) ||
			header.scene_info_size != sizeof(StaticScene::SceneInfo)) {
			throw std::runtime_error(
				"StaticScene file ABI does not match this build.");
		}
		if (header.payload_size != reader.remaining()) {
			throw std::runtime_error(
				"StaticScene file payload length is invalid.");
		}

#define X(type, name) reader.read(ret->name);
		SceneData_MACRO;
#undef X

		reader.read(ret->camera);
		reader.read(ret->environment_light);
		reader.read(ret->info);
		if (reader.remaining() != 0) {
			throw std::runtime_error(
				"StaticScene input contains trailing bytes.");
		}

		return ret;
	}
}
