#include "UsdCameraCompiler.hpp"

#include "SceneSpace.hpp"
#include "StaticSceneAssembler.hpp"

#include "FastJungle/scene/StaticScene.hpp"

#include <pxr/base/gf/frustum.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/camera.h>

namespace fjr::cooker::internal {

    void UsdCameraCompiler::compile(
        const pxr::UsdPrim& prim,
        const SceneSpace& scene_space,
        StaticSceneAssembler& assembler) {

        const pxr::UsdGeomCamera source{prim};
        scene::StaticScene::Camera camera;
        camera.name = assembler.intern_string(
            prim.GetName().GetString()).value();

        const auto source_camera = source.GetCamera(
            pxr::UsdTimeCode::Default());
        camera.world_transform = scene_space.camera_pose_to_target(
            source_camera.GetFrustum().ComputeViewInverse());
        source.GetFocalLengthAttr().Get(&camera.focal_length);
        source.GetHorizontalApertureAttr().Get(
            &camera.horizontal_aperture);
        source.GetVerticalApertureAttr().Get(
            &camera.vertical_aperture);
        source.GetHorizontalApertureOffsetAttr().Get(
            &camera.horizontal_aperture_offset);
        source.GetVerticalApertureOffsetAttr().Get(
            &camera.vertical_aperture_offset);

        float focus_distance = 0.0f;
        if (source.GetFocusDistanceAttr().Get(&focus_distance)) {
            camera.focus_distance = scene_space.distance_to_meters(
                focus_distance);
        }
        source.GetFStopAttr().Get(&camera.f_stop);

        pxr::GfVec2f clipping;
        if (source.GetClippingRangeAttr().Get(&clipping)) {
            camera.clipping_range = {
                scene_space.distance_to_meters(clipping[0]),
                scene_space.distance_to_meters(clipping[1]),
            };
        }
        assembler.set_camera(camera);
    }

} // namespace fjr::cooker::internal
