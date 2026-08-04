#pragma once

#include "FastJungle/dx12/Buffer.hpp"
#include "FastJungle/dx12/WindowsUtils.hpp"

#include <d3d12.h>

namespace fjr::dx {

	class UploadCBuffer : public dx::Buffer {

	public:
		template<typename T>
		void init(ID3D12Device* device, const T& src) {
			
			src_ = &src;
			struct_size_ = sizeof(T);

			constexpr size_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
			const size_t aligned_size = (sizeof(T) + alignment - 1) & ~(alignment - 1);

			Buffer::init(
				device,
				aligned_size,
				D3D12_HEAP_TYPE_UPLOAD,
				D3D12_RESOURCE_FLAG_NONE,
				D3D12_RESOURCE_STATE_GENERIC_READ);

			abort_failed(resource_->Map(0, nullptr, &mapped_));
		}
		
		void copy() {
			std::memcpy(mapped_, src_, struct_size_);
		}

	private:
		const void* src_ = nullptr;
		void* mapped_ = nullptr;
		size_t struct_size_ = 0;
	};
}