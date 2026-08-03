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
        scene.strings = {'\0', 't', 'e', 'x', '\0'};

        StaticScene::TextureMip mip;
        mip.width = 1;
        mip.height = 1;
        mip.row_pitch = 4;
        mip.slice_pitch = 4;
        mip.data_byte_offset_local = 0;
        scene.texture_mips.push_back(mip);

        StaticScene::Texture texture;
        texture.name = 1;
        texture.width = 1;
        texture.height = 1;
        texture.dxgi_format = 28;
        texture.mip_offset = 0;
        texture.mip_count = 1;
        texture.data_byte_offset = 0;
        texture.data_size = 4;
        scene.textures.push_back(texture);
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
    fjr::scene::StaticSceneWriter::save(
        streamed_path,
        scene,
        payload_path,
        payload.size());

    const auto metadata =
        fjr::scene::StaticSceneReader::load_metadata(streamed_path);
    require(
        metadata.texture_payload.file_offset != 0 &&
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
        "Streamed and memory outputs differ.");
    return 0;
}
