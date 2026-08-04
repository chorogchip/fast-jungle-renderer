#pragma once

#include <d3d12.h>

#include "FastJungle/dx12/DescriptorHeap.hpp"

namespace fjr::dx {

	class HeapManager {

	public:
		void init(
			ID3D12Device* device,
			UINT srv_cbv_uav_count,
			UINT sampler_count,
			UINT dsv_count,
			UINT rtv_count);
		void reset();

		dx::DescriptorHeap heap_srv_cbv_uav{};
		dx::DescriptorHeap heap_sampler{};
		dx::DescriptorHeap heap_dsv{};
		dx::DescriptorHeap heap_rtv{};

		static HeapManager g_heap_manager;
	};
}