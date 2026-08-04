#pragma once

#include <d3d12.h>

#include "FastJungle/renderer/Camera.hpp"

namespace fjr::render{

	class FrameData {

	public:
		void init(ID3D12Device* device);

		D3D12_GPU_VIRTUAL_ADDRESS get_camera_buffer() const;
		void upload_camera_data(const Camera& camera);
	};
}