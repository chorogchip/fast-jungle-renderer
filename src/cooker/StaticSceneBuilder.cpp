#include "FastJungle/cooker/StaticSceneBuilder.hpp"

#include "internal/JungleSceneCompiler.hpp"
#include "internal/OpenUsdRuntime.hpp"

namespace fjr::cooker {

    StaticSceneBuild StaticSceneBuilder::build(
        const std::filesystem::path& root_layer) {

        return internal::JungleSceneCompiler::compile(
            internal::OpenUsdRuntime::open_stage(root_layer));
    }

} // namespace fjr::cooker
