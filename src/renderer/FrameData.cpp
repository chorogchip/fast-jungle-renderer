#include "FastJungle/renderer/FrameData.hpp"

namespace fjr::render {

	void FrameData::init(ID3D12Device* device, const Camera& camera) {
		buffer_.init(device, camera);
	}

	D3D12_GPU_VIRTUAL_ADDRESS FrameData::get_camera_buffer() const {
		return buffer_->GetGPUVirtualAddress();
	}

	void FrameData::upload_camera_data() {
		buffer_.copy();
	}
}