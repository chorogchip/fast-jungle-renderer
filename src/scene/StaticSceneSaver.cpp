#include "FastJungle/scene/StaticSceneSaver.hpp"

#include <cstdio>
#include <filesystem>
#include <vector>

#include "FastJungle/scene/StaticSceneSerializer.hpp"

namespace fjr::scene {

    void StaticSceneSaver::save(const std::filesystem::path& path, const StaticScene& scene) {

        std::vector<std::byte> data(StaticSceneSerializer::calculate_length(scene));
        StaticSceneSerializer::serialize(data, scene);

        FILE* file = nullptr;
        _wfopen_s(*file, path.c_str(), L"wb");

        std::fwrite(data.data(), 1, data.size(), file);
        std::fclose(file);
    }

    std::unique_ptr<StaticScene> StaticSceneSaver::load(const std::filesystem::path& path) {

        FILE* file = nullptr;
        _wfopen_s(&file, path.c_str(), L"rb");

        std::fseek(file, 0, SEEK_END);
        std::vector<std::byte> data(std::ftell(file));
        std::rewind(file);

        std::fread(data.data(), 1, data.size(), file);
        std::fclose(file);

        return StaticSceneSerializer::deserialize(data);
    }

}  // namespace fjr::scene