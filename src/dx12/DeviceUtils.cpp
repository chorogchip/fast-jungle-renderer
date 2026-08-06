#include "FastJungle/dx12/DeviceUtils.hpp"

#include "FastJungle/core/util/EnumUtils.hpp"
#include "FastJungle/dx12/WindowsUtils.hpp"

namespace fjr::dx {

    Microsoft::WRL::ComPtr<IDXGIFactory4>
        DeviceUtils::create_factory() {
#if defined(_DEBUG)
        Microsoft::WRL::ComPtr<ID3D12Debug> debug_controller;

        if (SUCCEEDED(D3D12GetDebugInterface(
            IID_PPV_ARGS(debug_controller.ReleaseAndGetAddressOf())))) {
            debug_controller->EnableDebugLayer();
        }
#endif

        Microsoft::WRL::ComPtr<IDXGIFactory4> factory;

        abort_failed(CreateDXGIFactory1(
            IID_PPV_ARGS(factory.ReleaseAndGetAddressOf())));

        return factory;
    }

    Microsoft::WRL::ComPtr<ID3D12Device>
        DeviceUtils::create_device(
        IDXGIFactory4* factory) {

        for (UINT adapter_index = 0;; ++adapter_index) {
            Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;

            const HRESULT result = factory->EnumAdapters1(
                adapter_index,
                adapter.ReleaseAndGetAddressOf());

            if (result == DXGI_ERROR_NOT_FOUND) {
                break;
            }

            abort_failed(result);

            DXGI_ADAPTER_DESC1 description{};
            abort_failed(adapter->GetDesc1(&description));

            if (enm::has(
                description.Flags,
                static_cast<UINT>(DXGI_ADAPTER_FLAG_SOFTWARE))) {
                continue;
            }

            Microsoft::WRL::ComPtr<ID3D12Device> device;

            if (SUCCEEDED(D3D12CreateDevice(
                adapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(device.ReleaseAndGetAddressOf())))) {
                return device;
            }
        }

        Microsoft::WRL::ComPtr<IDXGIAdapter> warp_adapter;

        abort_failed(factory->EnumWarpAdapter(
            IID_PPV_ARGS(warp_adapter.ReleaseAndGetAddressOf())));

        Microsoft::WRL::ComPtr<ID3D12Device> device;

        abort_failed(D3D12CreateDevice(
            warp_adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(device.ReleaseAndGetAddressOf())));

        return device;
    }

} // namespace fjr::dx
