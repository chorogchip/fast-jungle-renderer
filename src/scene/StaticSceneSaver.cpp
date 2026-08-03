#include "FastJungle/scene/StaticSceneSaver.hpp"

#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <vector>

#include "FastJungle/scene/StaticSceneSerializer.hpp"
#include "FastJungle/scene/StaticSceneValidation.hpp"

namespace fjr::scene {

    void StaticSceneSaver::save(const std::filesystem::path& path, const StaticScene& scene) {

        validate_static_scene(scene);
        std::vector<std::byte> data(StaticSceneSerializer::calculate_length(scene));
        StaticSceneSerializer::serialize(data, scene);

        FILE* file = nullptr;
        const errno_t open_error = _wfopen_s(&file, path.c_str(), L"wb");
        if (open_error != 0 || file == nullptr) {
            throw std::runtime_error(
                "Failed to open scene output: " + path.generic_string());
        }

        const std::size_t written =
            std::fwrite(data.data(), 1, data.size(), file);
        const int close_result = std::fclose(file);
        if (written != data.size() || close_result != 0) {
            throw std::runtime_error(
                "Failed to write scene output: " + path.generic_string());
        }
    }

    std::unique_ptr<StaticScene> StaticSceneSaver::load(const std::filesystem::path& path) {

        FILE* file = nullptr;
        const errno_t open_error = _wfopen_s(&file, path.c_str(), L"rb");
        if (open_error != 0 || file == nullptr) {
            throw std::runtime_error(
                "Failed to open scene input: " + path.generic_string());
        }

        if (_fseeki64(file, 0, SEEK_END) != 0) {
            std::fclose(file);
            throw std::runtime_error(
                "Failed to seek scene input: " + path.generic_string());
        }

        const __int64 length = _ftelli64(file);
        if (length < 0) {
            std::fclose(file);
            throw std::runtime_error(
                "Failed to measure scene input: " + path.generic_string());
        }

        std::vector<std::byte> data(static_cast<std::size_t>(length));
        if (_fseeki64(file, 0, SEEK_SET) != 0) {
            std::fclose(file);
            throw std::runtime_error(
                "Failed to rewind scene input: " + path.generic_string());
        }

        const std::size_t read = std::fread(data.data(), 1, data.size(), file);
        const int close_result = std::fclose(file);
        if (read != data.size() || close_result != 0) {
            throw std::runtime_error(
                "Failed to read scene input: " + path.generic_string());
        }

        auto scene = StaticSceneSerializer::deserialize(data);
        validate_static_scene(*scene);
        return scene;
    }

}  // namespace fjr::scene
