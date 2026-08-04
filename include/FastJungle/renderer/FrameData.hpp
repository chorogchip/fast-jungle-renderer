#pragma once

#include <d3d12.h>

#include "FastJungle/dx12/UploadCBuffer.hpp"
#include "FastJungle/renderer/Camera.hpp"
#include "FastJungle/renderer/SceneResources.hpp"

namespace fjr::render {

	class FrameData {

	public:
		void init(ID3D12Device* device);

		D3D12_GPU_VIRTUAL_ADDRESS get_camera_buffer() const noexcept;
		void upload_camera_data(
			const Camera& camera,
			const scene::StaticScene::EnvironmentLight& environment);

	private:
		SceneResources::CameraConstants camera_constants_{};
		dx::UploadCBuffer buffer_;
	};
}