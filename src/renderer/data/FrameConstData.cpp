#include "FastJungle/renderer/data/FrameConstData.hpp"

#include "FastJungle/renderer/Camera.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render::data {

    FrameConstData FrameConstData::build(
        ID3D12Device* device,
        const Camera& camera,
        const scene::StaticScene::EnvironmentLight& environment) {

        FrameConstData ret{};
        auto& constants = ret.camera_constants.data();
        constants.view_projection = camera.get_view_projection_mat();
        constants.world_position = camera.get_position();
        constants.environment_world_transform = environment.world_transform;
        constants.environment_color = environment.color;
        constants.environment_intensity = environment.intensity
            * std::exp2(environment.exposure);
        constants.environment_texture_id = environment.texture;
    }

}  // namespace fjr::render::data