#include "StaticSceneBuilder.hpp"

#include <filesystem>
#include <utility>

#include "ImpostorBuilder.hpp"
#include "MeshLodBuilder.hpp"
#include "RasterClusterBuilder.hpp"
#include "TextureBuilder.hpp"
#include "FastJungle/core/util/File.hpp"
#include "FastJungle/scene/StaticSceneWriter.hpp"

#include "internal/JungleSceneBuilder.hpp"
#include "internal/OpenUsdRuntime.hpp"

namespace fjr::cooker {

    void StaticSceneBuilder::build() {
        constexpr wchar_t SCENE_NAME[] = L"JungleRuins.fjscene";
        const std::filesystem::path root_layer{FASTJUNGLE_DEFAULT_SCENE_USD};
        const std::filesystem::path output_path =
            std::filesystem::path{FASTJUNGLE_DEFAULT_COOKED_DIR} / SCENE_NAME;
        util::File::create_directories(output_path.parent_path());

        auto scene = internal::JungleSceneBuilder::build(
            internal::OpenUsdRuntime::open_stage(root_layer));
        MeshLodBuilder::build(*scene);
        RasterClusterBuilder::build(*scene);
        auto generated_textures = ImpostorBuilder::build(*scene);
        auto texture_payload =
            TextureBuilder::build(*scene, output_path, generated_textures);
        scene::StaticSceneWriter::save(output_path, *scene,
                                       std::move(texture_payload));
    }

} // namespace fjr::cooker
