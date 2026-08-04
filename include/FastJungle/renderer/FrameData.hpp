#pragma once

#include <d3d12.h>

#include "FastJungle/dx12/UploadCBuffer.hpp"
#include "FastJungle/renderer/Camera.hpp"

namespace fjr::render {

	class FrameData {

	public:
		void init(ID3D12Device* device, const Camera& camera);

		D3D12_GPU_VIRTUAL_ADDRESS get_camera_buffer() const;
		void upload_camera_data();

	private:
		dx::UploadCBuffer buffer_;
	};
}