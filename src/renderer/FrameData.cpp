#include "FastJungle/renderer/FrameData.hpp"

#include <cmath>

namespace fjr::render {

	void FrameData::init(ID3D12Device* device) {
		buffer_.init(device, this->camera_constants_);
	}

	D3D12_GPU_VIRTUAL_ADDRESS FrameData::get_camera_buffer() const noexcept {
		return buffer_->GetGPUVirtualAddress();
	}

	void FrameData::upload_camera_data(

        const Camera& camera,
        const scene::StaticScene::EnvironmentLight& environment) {

        camera_constants_.view_projection =
            camera.get_view_projection();

        camera_constants_.world_position =
            camera.get_world_position();

        camera_constants_.environment_world_transform =
            environment.world_transform;

        camera_constants_.environment_color =
            environment.color;

        camera_constants_.environment_intensity =
            environment.intensity *
            std::exp2(environment.exposure);

        camera_constants_.environment_texture_id =
            environment.texture;

		buffer_.copy();
	}
}