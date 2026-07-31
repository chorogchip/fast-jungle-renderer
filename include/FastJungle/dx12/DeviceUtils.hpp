#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace fjr::dx {

    class DeviceUtils {

    public:
        DeviceUtils() = delete;

        [[nodiscard]]
        Microsoft::WRL::ComPtr<IDXGIFactory4> create_factory();

        [[nodiscard]]
        Microsoft::WRL::ComPtr<ID3D12Device> create_device(
            IDXGIFactory4* factory);
    };

} // namespace fjr::dx