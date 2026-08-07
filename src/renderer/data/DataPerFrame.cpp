#include "FastJungle/renderer/data/DataPerFrame.hpp"

#include "FastJungle/renderer/Camera.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render::data {

    DataPerFrame DataPerFrame::build(
        ID3D12Device* device,
        const Camera& camera,
        const scene::StaticScene::EnvironmentLight& environment) {

        DataPerFrame ret{};
        ret.camera_constants.init(device);
        auto& buf = ret.camera_constants.data();

        buf.view_projection = camera.get_view_projection_mat();
        buf.world_position = camera.get_position();
        buf.environment_world_transform = environment.world_transform;
        buf.environment_color = environment.color;
        buf.environment_intensity = environment.intensity
            * std::exp2(environment.exposure);
        buf.environment_texture_id = environment.texture;

        return ret;
    }

}  // namespace fjr::render::data