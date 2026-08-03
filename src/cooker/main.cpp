
#include "FastJungle/core/util/Path.hpp"
#include "FastJungle/scene/StaticSceneSaver.hpp"

// FastJungleCooker.exe [JungleRuins_Karma.usda] [JungleRuins.fjscene]
int wmain() {

    using namespace fjr;

    util::Path path;
    auto scene = scene::StaticSceneSaver::load(path.native());

    return 0;
}
