#include "FastJungle/renderer/component/FrameData.hpp"

#include "FastJungle/renderer/Camera.hpp"

#include <cmath>

namespace fjr::render {

    void FrameData::init(ID3D12Device* device) {
        camera_buffer_.init(device);
    }

    D3D12_GPU_VIRTUAL_ADDRESS FrameData::get_camera_buffer() const noexcept {
        return camera_buffer_.get_address();
    }

    void FrameData::upload_camera_data(

        const Camera& camera,
        const scene::StaticScene::EnvironmentLight& environment) {

        auto& camera_constants = camera_buffer_.data();

        camera_constants.view_projection =
            camera.get_view_projection();

        camera_constants.world_position =
            camera.get_world_position();

        camera_constants.environment_world_transform =
            environment.world_transform;

        camera_constants.environment_color =
            environment.color;

        camera_constants.environment_intensity =
            environment.intensity *
            std::exp2(environment.exposure);

        camera_constants.environment_texture_id =
            environment.texture;
    }

} // namespace fjr::render
