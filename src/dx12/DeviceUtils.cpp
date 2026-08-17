#include "FastJungle/dx12/DeviceUtils.hpp"

#include "FastJungle/core/util/EnumUtils.hpp"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/WindowsUtils.hpp"

#include <string>
#include <utility>

namespace fjr::dx {

    namespace {

        [[nodiscard]]
        std::string to_utf8(const wchar_t* value) {

            const int byte_count = WideCharToMultiByte(
                CP_UTF8,
                0,
                value,
                -1,
                nullptr,
                0,
                nullptr,
                nullptr);
            if (byte_count <= 1) {
                return {};
            }

            std::string result(static_cast<std::size_t>(byte_count), '\0');
            WideCharToMultiByte(
                CP_UTF8,
                0,
                value,
                -1,
                result.data(),
                byte_count,
                nullptr,
                nullptr);
            result.pop_back();
            return result;
        }

        void log_selected_adapter(
            const DXGI_ADAPTER_DESC1& description,
            const char* selection_method) {

            constexpr std::size_t MEBIBYTE = 1024u * 1024u;

            log::Logger::g_logger_debug_out
                << "D3D12 adapter selected:\n"
                << "  name: " << to_utf8(description.Description) << '\n'
                << "  selection: " << selection_method << '\n'
                << "  vendor ID: " << description.VendorId << '\n'
                << "  device ID: " << description.DeviceId << '\n'
                << "  dedicated video memory: "
                << description.DedicatedVideoMemory / MEBIBYTE << " MiB\n"
                << "  dedicated system memory: "
                << description.DedicatedSystemMemory / MEBIBYTE << " MiB\n"
                << "  shared system memory: "
                << description.SharedSystemMemory / MEBIBYTE << " MiB\n"
                << "  software: "
                << (enm::has(
                    description.Flags,
                    static_cast<UINT>(DXGI_ADAPTER_FLAG_SOFTWARE))
                    ? "yes"
                    : "no")
                << '\n';
            log::Logger::g_logger_debug_out.flush_debug_string();
        }

        [[nodiscard]]
        Microsoft::WRL::ComPtr<ID3D12Device> try_create_device(
            IDXGIAdapter1* adapter) {

            Microsoft::WRL::ComPtr<ID3D12Device> device;
            if (FAILED(D3D12CreateDevice(
                adapter,
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(device.ReleaseAndGetAddressOf())))) {
                return {};
            }

            return device;
        }

        [[nodiscard]]
        Microsoft::WRL::ComPtr<ID3D12Device>
            create_high_performance_device(
                IDXGIFactory4* factory,
                std::source_location loc) {

            Microsoft::WRL::ComPtr<IDXGIFactory6> factory6;
            if (FAILED(factory->QueryInterface(
                IID_PPV_ARGS(factory6.ReleaseAndGetAddressOf())))) {
                return {};
            }

            for (UINT adapter_index = 0;; ++adapter_index) {
                Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
                const HRESULT result = factory6->EnumAdapterByGpuPreference(
                    adapter_index,
                    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                    IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()));
                if (result == DXGI_ERROR_NOT_FOUND) {
                    break;
                }
                abort_failed(result, loc);

                DXGI_ADAPTER_DESC1 description{};
                abort_failed(adapter->GetDesc1(&description), loc);
                if (enm::has(
                    description.Flags,
                    static_cast<UINT>(DXGI_ADAPTER_FLAG_SOFTWARE))) {
                    continue;
                }

                auto device = try_create_device(
                    adapter.Get());
                if (device) {
                    log_selected_adapter(
                        description,
                        "DXGI high-performance preference");
                    return device;
                }
            }

            return {};
        }

        [[nodiscard]]
        Microsoft::WRL::ComPtr<ID3D12Device>
            create_best_legacy_device(
                IDXGIFactory4* factory,
                std::source_location loc) {

            Microsoft::WRL::ComPtr<ID3D12Device> best_device;
            std::size_t best_dedicated_video_memory = 0;
            DXGI_ADAPTER_DESC1 best_description{};

            for (UINT adapter_index = 0;; ++adapter_index) {
                Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
                const HRESULT result = factory->EnumAdapters1(
                    adapter_index,
                    adapter.ReleaseAndGetAddressOf());
                if (result == DXGI_ERROR_NOT_FOUND) {
                    break;
                }
                abort_failed(result, loc);

                DXGI_ADAPTER_DESC1 description{};
                abort_failed(adapter->GetDesc1(&description), loc);
                if (enm::has(
                    description.Flags,
                    static_cast<UINT>(DXGI_ADAPTER_FLAG_SOFTWARE))) {
                    continue;
                }

                auto device = try_create_device(
                    adapter.Get());
                if (!device ||
                    description.DedicatedVideoMemory <
                    best_dedicated_video_memory) {
                    continue;
                }

                best_dedicated_video_memory =
                    description.DedicatedVideoMemory;
                best_description = description;
                best_device = std::move(device);
            }

            if (best_device) {
                log_selected_adapter(
                    best_description,
                    "legacy enumeration; largest dedicated video memory");
            }
            return best_device;
        }

    } // namespace

    Microsoft::WRL::ComPtr<IDXGIFactory4>
        DeviceUtils::create_factory(std::source_location loc) {
#if defined(_DEBUG)
        Microsoft::WRL::ComPtr<ID3D12Debug> debug_controller;

        if (SUCCEEDED(D3D12GetDebugInterface(
            IID_PPV_ARGS(debug_controller.ReleaseAndGetAddressOf())))) {
            debug_controller->EnableDebugLayer();
        }
#endif

        Microsoft::WRL::ComPtr<IDXGIFactory4> factory;

        abort_failed(CreateDXGIFactory1(
            IID_PPV_ARGS(factory.ReleaseAndGetAddressOf())),
            loc);

        return factory;
    }

    Microsoft::WRL::ComPtr<ID3D12Device>
        DeviceUtils::create_device(
            IDXGIFactory4* factory,
            std::source_location loc) {

        if (auto device = create_high_performance_device(factory, loc)) {
            return device;
        }

        if (auto device = create_best_legacy_device(factory, loc)) {
            return device;
        }

        Microsoft::WRL::ComPtr<IDXGIAdapter> warp_adapter;

        abort_failed(factory->EnumWarpAdapter(
            IID_PPV_ARGS(warp_adapter.ReleaseAndGetAddressOf())),
            loc);

        Microsoft::WRL::ComPtr<IDXGIAdapter1> warp_adapter1;
        abort_failed(warp_adapter->QueryInterface(
            IID_PPV_ARGS(warp_adapter1.ReleaseAndGetAddressOf())),
            loc);

        DXGI_ADAPTER_DESC1 warp_description{};
        abort_failed(warp_adapter1->GetDesc1(&warp_description), loc);
        auto device = try_create_device(
            warp_adapter1.Get());
        if (device) {
            log_selected_adapter(
                warp_description,
                "WARP fallback (no hardware D3D12 adapter was usable)");
        }
        if (!device) {
            dx::abort_failed(D3D12CreateDevice(
                warp_adapter1.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(device.ReleaseAndGetAddressOf())),
                loc);
        }

        return device;
    }

    static void check_feature_support(
        ID3D12Device* device) {

        // feature support SM 6.6 for SW raster

        D3D12_FEATURE_DATA_D3D12_OPTIONS1 options1{};
        D3D12_FEATURE_DATA_SHADER_MODEL shader_model{ D3D_SHADER_MODEL_6_6 };

        abort_failed(device->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS1, &options1, sizeof(options1)));
        abort_failed(device->CheckFeatureSupport(
            D3D12_FEATURE_SHADER_MODEL,
            &shader_model,
            sizeof(shader_model)));

        log::Logger::g_logger <<
            log::asrt(options1.Int64ShaderOps) <<
            log::asrt(
                shader_model.HighestShaderModel >= D3D_SHADER_MODEL_6_6);
    }

} // namespace fjr::dx
