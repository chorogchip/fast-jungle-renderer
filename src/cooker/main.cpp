
#include <exception>
#include <filesystem>
#include <memory>
#include <string>

#ifndef FASTJUNGLE_DEFAULT_SCENE_USD
#define FASTJUNGLE_DEFAULT_SCENE_USD ""
#endif

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/core/util/Path.hpp"
#include "FastJungle/cooker/StaticSceneBuilder.hpp"
#include "FastJungle/scene/StaticSceneSaver.hpp"



// FastJungleCooker.exe [JungleRuins_Karma.usda] [JungleRuins.fjscene]
int wmain(int argc, wchar_t** argv) {

    using namespace fjr;

    const std::filesystem::path root_layer = argc >= 2
        ? std::filesystem::path{ argv[1] }
    : std::filesystem::path{ FASTJUNGLE_DEFAULT_SCENE_USD };

    const auto scene = fjr::cooker::StaticSceneBuilder::build(root_layer);

    auto& logger = fjr::log::Logger::g_logger;
    logger
        << "StaticScene built\n"
        << "  vertices: " << scene->vertices.size() << '\n'
        << "  indices: " << scene->indices.size() << '\n'
        << "  textures: " << scene->textures.size() << '\n'
        << "  materials: " << scene->materials.size() << '\n'
        << "  meshes: " << scene->meshes.size() << '\n'
        << "  prototypes: " << scene->prototypes.size() << '\n'
        << "  point batches: " << scene->point_batches.size() << '\n'
        << "  point instances: " << scene->point_instances.size() << '\n'
        << "  matrix batches: " << scene->matrix_batches.size() << '\n'
        << "  matrix instances: " << scene->matrix_instances.size() << '\n';
    logger.flush();

    util::Path path{ "asd" };
    scene::StaticSceneSaver::save(path.native(), *scene.get());

    return 0;
}
