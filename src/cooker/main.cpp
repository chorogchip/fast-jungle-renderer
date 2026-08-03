
#include "FastJungle/cooker/JungleUsdImporter.hpp"
#include "FastJungle/cooker/JungleSceneReport.hpp"
#include "FastJungle/cooker/StaticSceneBuilder.hpp"

// FastJungleCooker.exe [JungleRuins_Karma.usda] [JungleRuins.fjscene]
int wmain() {

    const auto scene = fjr::cooker::JungleUsdImporter::
        import_scene(FASTJUNGLE_DEFAULT_SCENE_USD);

    fjr::cooker::JungleSceneReport::
        report(scene);

    auto static_scene = fjr::cooker::StaticSceneBuilder::
        build(scene);

    // std::filesystem::path{ FASTJUNGLE_DEFAULT_COOKED_DIR } / "JungleRuins.fjscene";
    return 0;
}
