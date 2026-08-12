#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <source_location>

namespace fjr::dx {

    class DeviceUtils {

    public:
        DeviceUtils() = delete;

        [[nodiscard]]
        static Microsoft::WRL::ComPtr<IDXGIFactory4> create_factory(
            std::source_location loc =
                std::source_location::current());

        [[nodiscard]]
        static Microsoft::WRL::ComPtr<ID3D12Device> create_device(
            IDXGIFactory4* factory,
            std::source_location loc =
                std::source_location::current());
    };

} // namespace fjr::dx
