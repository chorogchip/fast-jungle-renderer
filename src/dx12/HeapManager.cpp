#include "FastJungle/dx12/HeapManager.hpp"

namespace fjr::dx {

	HeapManager HeapManager::g_heap_manager{};

	void HeapManager::init(
		ID3D12Device* device,
		UINT srv_cbv_uav_count,
		UINT sampler_count,
		UINT dsv_count,
		UINT rtv_count) {

		heap_srv_cbv_uav.init(device,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			srv_cbv_uav_count,
			true);

		heap_sampler.init(device,
			D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
			sampler_count,
			true);

		heap_dsv.init(
			device,
			D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
			dsv_count,
			false);

		heap_rtv.init(
			device,
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			rtv_count,
			false);
	}

	void HeapManager::reset() {
		heap_srv_cbv_uav.reset();
		heap_sampler.reset();
		heap_dsv.reset();
		heap_rtv.reset();
	}

}