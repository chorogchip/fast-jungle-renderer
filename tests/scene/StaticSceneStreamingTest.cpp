#include <Windows.h>

#include "FastJungle/core/util/File.hpp"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/scene/StaticSceneReader.hpp"
#include "FastJungle/scene/StaticSceneWriter.hpp"

#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

    class TemporaryDirectory final {
    public:
        TemporaryDirectory() {
            path_ = std::filesystem::temp_directory_path() /
                ("FastJungleSceneStreamingTest-" +
                    std::to_string(GetCurrentProcessId()));
            std::error_code error;
            std::filesystem::remove_all(path_, error);
            error.clear();
            std::filesystem::create_directories(path_, error);
            if (error) {
                fjr::log::Logger::g_logger
                    << "Failed to create test directory: "
                    << path_ << '\n';
                fjr::log::Logger::g_logger.abort();
            }
        }

        ~TemporaryDirectory() {
            std::error_code error;
            std::filesystem::remove_all(path_, error);
        }

        TemporaryDirectory(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

        [[nodiscard]]
        const std::filesystem::path& path() const noexcept {
            return path_;
        }

    private:
        std::filesystem::path path_;
    };

    [[noreturn]]
    void fail(std::string_view message) {
        fjr::log::Logger::g_logger << message << '\n';
        fjr::log::Logger::g_logger.abort();
    }

    void require(bool condition, std::string_view message) {
        if (!condition) {
            fail(message);
        }
    }

    template<typename T>
    void require_equal(
        const std::vector<T>& expected,
        const std::vector<T>& actual,
        std::string_view name) {

        if (expected.size() != actual.size() ||
            (!expected.empty() && std::memcmp(
                expected.data(),
                actual.data(),
                expected.size() * sizeof(T)) != 0)) {
            fjr::log::Logger::g_logger
                << "StaticScene field changed: " << name << '\n';
            fjr::log::Logger::g_logger.abort();
        }
    }

    void require_equal(
        const fjr::scene::StaticScene& expected,
        const fjr::scene::StaticScene& actual) {

#define X(type, name) require_equal(expected.name, actual.name, #name);
        SceneData_MACRO
#undef X
        require_equal(
            expected.texture_data,
            actual.texture_data,
            "texture_data");

		require(
			std::memcmp(
				&expected.components,
				&actual.components,
				sizeof(expected.components)) == 0,
			"StaticScene components changed.");
        require(
            std::memcmp(
                &expected.camera,
                &actual.camera,
                sizeof(expected.camera)) == 0,
            "StaticScene camera changed.");
        require(
            std::memcmp(
                &expected.environment_light,
                &actual.environment_light,
                sizeof(expected.environment_light)) == 0,
            "StaticScene environment light changed.");
        require(
            std::memcmp(
                &expected.info,
                &actual.info,
                sizeof(expected.info)) == 0,
            "StaticScene info changed.");
    }

    [[nodiscard]]
    fjr::scene::StaticScene make_scene() {
        using StaticScene = fjr::scene::StaticScene;

        StaticScene scene;
        scene.strings = {
            '\0',
            'g', '\0',
            'l', '\0',
            'p', '\0',
            't', 'e', 'x', '\0'
        };
        StaticScene::TextureMip mip;
        mip.width = 1;
        mip.height = 1;
        mip.row_pitch = 4;
        mip.slice_pitch = 4;
        mip.data_byte_offset_local = 0;
        scene.texture_mips.push_back(mip);

        StaticScene::Texture texture;
        texture.name = 7;
        texture.width = 1;
        texture.height = 1;
        texture.dxgi_format = 28;
        texture.mip_offset = 0;
        texture.mip_count = 1;
        texture.data_byte_offset = 0;
        texture.data_size = 4;
        scene.textures.push_back(texture);

		scene.samplers.emplace_back();
		scene.texture_bindings.push_back({
			.texture = 0,
			.sampler = 0,
			.channel = StaticScene::EnumTextureChannel::RGB,
			.flags = StaticScene::EnumTextureBindingFlag::SRGB,
		});

        StaticScene::Material material;
        material.name = 1;
        material.ior = 1.45f;
        material.specular = 0.355f;
        material.clearcoat = 0.25f;
        material.clearcoat_roughness = 0.03f;
		material.texture_binding_base_color = 0;
        scene.materials.push_back(material);

        scene.vertices = {
            {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        };
        scene.indices = {0, 1, 2};
        scene.submeshes.push_back({
            .name = 1,
            .vertex_offset = 0,
            .vertex_count = 3,
            .index_offset = 0,
            .index_count = 3,
            .material = 0,
        });
        scene.mesh_lods.push_back({
            .submesh_offset = 0,
            .submesh_count = 1,
        });
        scene.meshes.push_back({
            .name = 1,
            .lod_offset = 0,
            .lod_count = 1,
        });

        scene.triangle_bool_streams.push_back({
            .mesh = 0,
            .name = 1,
            .value_offset = 0,
            .value_count = 1,
        });
        scene.triangle_bool_values.push_back(1);
        scene.corner_float_streams.push_back({
            .mesh = 0,
            .name = 1,
            .source_interpolation =
                StaticScene::EnumAttributeInterpolation::VERTEX,
            .value_offset = 0,
            .value_count = 3,
        });
        scene.corner_float_values = {0.0f, 0.5f, 1.0f};
        scene.corner_color3_streams.push_back({
            .mesh = 0,
            .name = 1,
            .source_interpolation =
                StaticScene::EnumAttributeInterpolation::FACE_VARYING,
            .value_offset = 0,
            .value_count = 3,
        });
        scene.corner_color3_values = {
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
        };
        scene.corner_texcoord2_streams.push_back({
            .mesh = 0,
            .name = 1,
            .source_interpolation =
                StaticScene::EnumAttributeInterpolation::FACE_VARYING,
            .value_offset = 0,
            .value_count = 3,
        });
        scene.corner_texcoord2_values = {
            {0.0f, 0.0f},
            {1.0f, 0.0f},
            {0.0f, 1.0f},
        };

        scene.instanced_mesh_definitions.push_back({.mesh = 0});
        for (std::uint32_t index = 0; index < 12; ++index) {
            scene.point_instances.emplace_back();
            scene.point_batches.push_back({
                .name = 1,
                .definition = 0,
                .instance_offset = index,
                .instance_count = 1,
            });
        }
        scene.components.anthurium.point_batches = {0, 1};
        scene.components.nettle.point_batches = {1, 1};
        scene.components.shrub_sorrel.point_batches = {2, 1};
        scene.components.shrub.point_batches = {3, 1};
        scene.components.grass_b.point_batches = {4, 1};
        scene.components.grass_a.point_batches = {5, 1};
        scene.components.pyramid_grass_b.point_batches = {6, 1};
        scene.components.pyramid_moss.point_batches = {7, 1};
        scene.components.queen_forest.point_batches = {8, 1};
        scene.components.river_forest.point_batches = {9, 1};
        scene.components.river_sapling.point_batches = {10, 1};
        scene.components.river_seedling.point_batches = {11, 1};

        for (std::uint32_t index = 0; index < 6; ++index) {
            scene.static_mesh_instances.push_back({
                .name = 1,
                .mesh = 0,
            });
        }
        scene.components.pyramid.instance = 0;
        scene.components.river.instance = 1;
        scene.components.creek.instance = 2;
        scene.components.banyan.instance = 3;
        scene.components.terrain.extended = {4, 1};
        scene.components.terrain.cinematic = {5, 1};
        scene.camera.name = 1;
        scene.environment_light.name = 1;
        scene.info.vertex_count_before_indexing = 3;
        scene.info.vertex_count_after_indexing = 3;
        return scene;
    }

    [[nodiscard]]
    std::vector<std::byte> read_file(
        const std::filesystem::path& path) {

        std::vector<std::byte> result;
        try {
            result.resize(fjr::util::File::size(path));
        }
        catch (...) {
            fail("Failed to allocate test file buffer.");
        }

        auto input = fjr::util::File::open_read(path);
        input.read(
            reinterpret_cast<char*>(result.data()),
            static_cast<std::streamsize>(result.size()));
        require(
            input && input.peek() == std::char_traits<char>::eof(),
            "Failed to read test file.");
        return result;
    }

} // namespace

int main(int argc, char** argv) {
    if (argc == 2) {
        const auto scene = fjr::scene::StaticSceneReader::load(argv[1]);
        std::cout
            << "Loaded StaticScene: "
            << scene->textures.size() << " textures, "
            << scene->texture_data.size() << " texture bytes\n";
        return 0;
    }
    require(argc == 1, "Unexpected test arguments.");

    const TemporaryDirectory directory;
    const auto payload_path = directory.path() / "texture.bin";
    const auto streamed_path = directory.path() / "streamed.fjscene";
    const auto memory_path = directory.path() / "memory.fjscene";
    const std::array payload{
        std::byte{0x10},
        std::byte{0x20},
        std::byte{0x30},
        std::byte{0x40}
    };

    auto payload_output = fjr::util::File::open_write(payload_path);
    payload_output.write(
        reinterpret_cast<const char*>(payload.data()),
        static_cast<std::streamsize>(payload.size()));
    fjr::util::File::finish(payload_output, payload_path);

    auto scene = make_scene();
	require(
		scene.texture_data.empty(),
		"Fresh test scene unexpectedly contains texture data.");
    fjr::scene::StaticSceneWriter::save(
        streamed_path,
        scene,
        payload_path,
        payload.size());

    const auto metadata =
        fjr::scene::StaticSceneReader::load_metadata(streamed_path);
    require(
        metadata.texture_payload.path ==
            fjr::scene::StaticSceneReader::texture_path(streamed_path) &&
        metadata.texture_payload.file_offset ==
            read_file(metadata.texture_payload.path).size() - payload.size() &&
        metadata.texture_payload.size == payload.size(),
        "Texture payload range changed.");
    require_equal(scene, *metadata.scene);

    const auto loaded =
        fjr::scene::StaticSceneReader::load(streamed_path);
    scene.texture_data.assign(payload.begin(), payload.end());
    require_equal(scene, *loaded);

    fjr::scene::StaticSceneWriter::save(memory_path, scene);
    require(
        read_file(streamed_path) == read_file(memory_path),
        "Streamed and memory scene outputs differ.");
    require(
        read_file(fjr::scene::StaticSceneReader::texture_path(streamed_path)) ==
            read_file(fjr::scene::StaticSceneReader::texture_path(memory_path)),
        "Streamed and memory texture outputs differ.");
    return 0;
}
