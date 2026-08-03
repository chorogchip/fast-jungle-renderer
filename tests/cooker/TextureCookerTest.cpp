#include <Windows.h>
#include <dxgiformat.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "FastJungle/cooker/TextureCooker.hpp"
#include "FastJungle/scene/StaticSceneValidation.hpp"

namespace {

    class TemporaryDirectory final {
    public:
        TemporaryDirectory() {
            path_ = std::filesystem::temp_directory_path() /
                ("FastJungleTextureCookerTest-" +
                    std::to_string(GetCurrentProcessId()));
            std::error_code error;
            std::filesystem::remove_all(path_, error);
            error.clear();
            std::filesystem::create_directories(path_, error);
            if (error) {
                throw std::runtime_error(
                    "Failed to create the test directory.");
            }
        }

        ~TemporaryDirectory() {
            std::error_code error;
            std::filesystem::remove_all(path_, error);
        }

        TemporaryDirectory(const TemporaryDirectory&) = delete;
        TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

        [[nodiscard]] const std::filesystem::path& path() const noexcept {
            return path_;
        }

    private:
        std::filesystem::path path_;
    };

    void write_tga(
        const std::filesystem::path& path,
        std::uint16_t width,
        std::uint16_t height) {

        std::vector<std::uint8_t> data(
            18u + static_cast<std::size_t>(width) * height * 4u);
        data[2] = 2;
        data[12] = static_cast<std::uint8_t>(width);
        data[13] = static_cast<std::uint8_t>(width >> 8u);
        data[14] = static_cast<std::uint8_t>(height);
        data[15] = static_cast<std::uint8_t>(height >> 8u);
        data[16] = 32;
        data[17] = 0x28;
        for (std::size_t pixel = 18; pixel < data.size(); pixel += 4) {
            data[pixel] = 0x10;
            data[pixel + 1] = 0x20;
            data[pixel + 2] = 0x30;
            data[pixel + 3] = 0xff;
        }

        std::ofstream output{path, std::ios::binary | std::ios::trunc};
        output.write(
            reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
        if (!output) {
            throw std::runtime_error("Failed to write the test texture.");
        }
    }

    [[nodiscard]] fjr::scene::StaticScene make_scene() {
        fjr::scene::StaticScene scene;
        scene.strings = {'\0', 't', 'e', 'x', '\0'};
        fjr::scene::StaticScene::Texture texture;
        texture.name = 1;
        scene.textures.push_back(texture);
        return scene;
    }

} // namespace

int main() {
    const TemporaryDirectory directory;
    const auto texture_path = directory.path() / "texture.tga";
    const auto payload_path = directory.path() / "texture.bin";
    write_tga(texture_path, 8, 8);

    const std::array texture_paths{texture_path.generic_string()};
    auto scene = make_scene();
    const std::uint64_t payload_size = fjr::cooker::TextureCooker::cook(
        scene,
        texture_paths,
        payload_path);
    if (payload_size != 112 ||
        scene.texture_data.size() != 0 ||
        scene.texture_mips.size() != 4 ||
        scene.textures.front().width != 8 ||
        scene.textures.front().height != 8 ||
        scene.textures.front().dxgi_format != DXGI_FORMAT_BC7_UNORM ||
        scene.textures.front().mip_count != 4 ||
        scene.textures.front().data_size != 112 ||
        std::filesystem::file_size(payload_path) != 112) {
        throw std::runtime_error("Texture cook result is invalid.");
    }
    const auto& smallest_mip = scene.texture_mips.back();
    if (smallest_mip.width != 1 ||
        smallest_mip.height != 1 ||
        smallest_mip.row_pitch != 16 ||
        smallest_mip.slice_pitch != 16) {
        throw std::runtime_error("Texture mip chain is invalid.");
    }
    fjr::scene::StaticSceneValidator::validate(scene, payload_size);

    auto limited_scene = make_scene();
    const auto limited_payload_path =
        directory.path() / "limited-texture.bin";
    bool rejected = false;
    try {
        (void)fjr::cooker::TextureCooker::cook(
            limited_scene,
            texture_paths,
            limited_payload_path,
            {.maximum_decoded_texture_bytes = 3});
    }
    catch (const std::runtime_error&) {
        rejected = true;
    }
    if (!rejected || std::filesystem::exists(limited_payload_path)) {
        throw std::runtime_error(
            "Texture memory budget was not enforced.");
    }
    return 0;
}
