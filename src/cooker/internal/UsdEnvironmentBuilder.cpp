#include "UsdEnvironmentBuilder.hpp"

#include "SceneSpace.hpp"
#include "StaticSceneDataBuilder.hpp"

#include "FastJungle/scene/StaticScene.hpp"

#include <pxr/base/gf/vec3f.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usdLux/domeLight.h>
#include <pxr/usd/usdLux/lightAPI.h>

#include <filesystem>

namespace fjr::cooker::internal {

    void UsdEnvironmentBuilder::build(
        const pxr::UsdPrim& prim,
        DirectX::FXMMATRIX world_transform,
        StaticSceneDataBuilder& scene_builder) {

        const pxr::UsdLuxDomeLight dome{prim};
        const pxr::UsdLuxLightAPI light{prim};
        scene::StaticScene::EnvironmentLight destination;
        destination.name = scene_builder.intern_string(
            prim.GetName().GetString()).value();
        destination.world_transform = SceneSpace::store(world_transform);

        pxr::GfVec3f color{1.0f};
        light.GetColorAttr().Get(&color);
        destination.color = {color[0], color[1], color[2]};
        light.GetIntensityAttr().Get(&destination.intensity);
        light.GetExposureAttr().Get(&destination.exposure);

        pxr::SdfAssetPath texture;
        if (dome.GetTextureFileAttr().Get(&texture) &&
            !texture.GetResolvedPath().empty()) {
            destination.texture = scene_builder.intern_texture(
                std::filesystem::path{
                    texture.GetResolvedPath()}).value();
        }
        scene_builder.set_environment_light(destination);
    }

} // namespace fjr::cooker::internal
