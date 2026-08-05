#include "FastJungle/renderer/builder/SceneFrameConstDataBuilder.hpp"

#include <cmath>
#include <stdexcept>

namespace fjr::render {

    void SceneFrameConstDataBuilder::build(
        data::FrameConstData& output,
        ID3D12Device* device,
        const Camera& camera,
        const scene::StaticScene::
        EnvironmentLight& environment) {

        if (device == nullptr) {
            throw std::invalid_argument(
                "SceneFrameConstDataBuilder requires "
                "a D3D12 device.");
        }

        if (!output.initialized) {
            output.camera_constants.init(device);
            output.initialized = true;
        }

        auto& constants =
            output.camera_constants.data();

        constants.view_projection =
            camera.get_view_projection_mat();

        constants.world_position =
            camera.get_position();

        constants.environment_world_transform =
            environment.world_transform;

        constants.environment_color =
            environment.color;

        constants.environment_intensity =
            environment.intensity *
            std::exp2(environment.exposure);

        constants.environment_texture_id =
            environment.texture;
    }

} // namespace fjr::render