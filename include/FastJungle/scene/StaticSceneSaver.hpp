
#include <filesystem>
#include <vector>
#include <memory>

#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::scene {

    class StaticSceneSaver {

    public:
        static void save(const std::filesystem::path& path, const StaticScene& scene);
        static std::unique_ptr<StaticScene> load(const std::filesystem::path& path);
    };
}