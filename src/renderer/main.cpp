
#include <Windows.h>
#include <shellapi.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "FastJungle/core/util/Assume.h"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/renderer/Application.hpp"
#include "FastJungle/scene/StaticSceneReader.hpp"

namespace {

    fjr::Application g_application;

    struct RendererLaunchOptions {
        std::filesystem::path scene = std::filesystem::path{ FASTJUNGLE_DEFAULT_COOKED_DIR } / "JungleRuins.fjscene";
        fjr::render::RendererOptions renderer;
        std::optional<std::filesystem::path> benchmark_output;
        std::uint32_t benchmark_warmup_frames = 60;
        std::uint32_t benchmark_frames = 240;
    };

    void write_benchmark(
        const std::filesystem::path& path,
        const std::vector<double>& samples) {

        if (samples.empty()) return;

        std::error_code error;
        if (!path.parent_path().empty()) {
            std::filesystem::create_directories(
                path.parent_path(),
                error);
        }

        auto sorted = samples;
        std::ranges::sort(sorted);

        const auto percentile = [&](double value) {
            const auto index = static_cast<std::size_t>(
                value * static_cast<double>(sorted.size() - 1));
            return sorted[index];
        };

        const double total = std::accumulate(
            samples.begin(),
            samples.end(),
            0.0);

        const double mean = total /
            static_cast<double>(samples.size());

        std::ofstream output{ path, std::ios::trunc };
        output
            << "frames,mean_ms,p50_ms,p95_ms,min_ms,max_ms,fps\n"
            << samples.size() << ','
            << std::fixed << std::setprecision(6)
            << mean << ','
            << percentile(0.50) << ','
            << percentile(0.95) << ','
            << sorted.front() << ','
            << sorted.back() << ','
            << 1000.0 / mean << '\n';
    }

    RendererLaunchOptions get_renderer_launch_options() {
        RendererLaunchOptions options;

        std::error_code path_error;
        if (!std::filesystem::exists(options.scene, path_error)) {
            const auto bc_scene =
                std::filesystem::path{ FASTJUNGLE_DEFAULT_COOKED_DIR } /
                "JungleRuins-release-bc.fjscene";
            if (std::filesystem::exists(bc_scene, path_error)) {
                options.scene = bc_scene;
            }
        }

        int argc = 0;
        wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv == nullptr) return options;

        for (int index = 1; index < argc; ++index) {
            const std::wstring_view argument{ argv[index] };

            if (argument == L"--scene" && index + 1 < argc) {
                options.scene = argv[++index];

            } else if (argument == L"--force-lod0") {
                options.renderer.lod_selection = fjr::render::LodSelectionMode::FINEST;

            } else if (argument == L"--force-coarsest-lod") {
                options.renderer.lod_selection = fjr::render::LodSelectionMode::COARSEST;

            } else if (argument == L"--overview") {
                options.renderer.frame_entire_scene = true;

            } else if (argument == L"--no-vsync") {
                options.renderer.vsync = false;

            } else if (argument == L"--benchmark-output" &&
                index + 1 < argc) {

                options.benchmark_output = argv[++index];
                options.renderer.vsync = false;

            } else if (argument == L"--benchmark-warmup" &&
                index + 1 < argc) {

                options.benchmark_warmup_frames =
                    static_cast<std::uint32_t>(
                        std::wcstoul(argv[++index], nullptr, 10));

            } else if (argument == L"--benchmark-frames" &&
                index + 1 < argc) {

                options.benchmark_frames =
                    static_cast<std::uint32_t>(
                        std::wcstoul(argv[++index], nullptr, 10));

            } else if (argument == L"--demo-pyramid") {
                options.renderer.objects.river_seedling = false;
                options.renderer.objects.river_forest = false;
                options.renderer.objects.pyramid_moss = false;
                options.renderer.objects.other_foliage = false;
                options.renderer.objects.terrain = false;

            } else if (argument == L"--demo-foliage") {
                options.renderer.objects.terrain = false;
                options.renderer.objects.other = false;

            } else if (argument == L"--demo-basic") {
                options.renderer.objects.river_seedling = false;
                options.renderer.objects.river_forest = false;
                options.renderer.objects.pyramid_moss = false;
                options.renderer.objects.other_foliage = false;

            } else if (argument == L"--no-river-seedling") {
                options.renderer.objects.river_seedling = false;

            } else if (argument == L"--no-river-forest") {
                options.renderer.objects.river_forest = false;

            } else if (argument == L"--no-pyramid-moss") {
                options.renderer.objects.pyramid_moss = false;

            } else if (argument == L"--no-other-foliage") {
                options.renderer.objects.other_foliage = false;

            } else if (argument == L"--no-terrain") {
                options.renderer.objects.terrain = false;

            } else if (argument == L"--no-other") {
                options.renderer.objects.other = false;

            }
        }

        LocalFree(argv);
        return options;
    }

    LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {

        switch (message) {

        case WM_SIZE:
            g_application.resize(
                static_cast<UINT>(LOWORD(lparam)),
                static_cast<UINT>(HIWORD(lparam)));
            return 0;

        case WM_KEYDOWN:
            g_application.handle_key_down(
                static_cast<std::uint32_t>(wparam));
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, message, wparam, lparam);
        }
    }

    HWND create_window(HINSTANCE instance, UINT width, UINT height) {

        constexpr wchar_t class_name[] = L"FastJungleWindow";

        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.style = 0;
        window_class.lpfnWndProc = window_proc;
        window_class.hInstance = instance;
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.lpszClassName = class_name;

        if (RegisterClassExW(&window_class) == 0)
            return nullptr;

        RECT rectangle{ 0, 0,
            static_cast<LONG>(width),
            static_cast<LONG>(height)
        };

        if (!AdjustWindowRect(
            &rectangle,
            WS_OVERLAPPEDWINDOW,
            FALSE)) {
            return nullptr;
        }

        return CreateWindowExW(
            0,
            class_name,
            L"Fast Jungle",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rectangle.right - rectangle.left,
            rectangle.bottom - rectangle.top,
            nullptr,
            nullptr,
            instance,
            0);
    }

} // namespace

int WINAPI wWinMain(
    _In_ HINSTANCE instance,
    _In_opt_ HINSTANCE,
    _In_ PWSTR,
    _In_ int show_command) {

    auto options = get_renderer_launch_options();

    /*
    
    struct RendererLaunchOptions {
        std::filesystem::path scene = std::filesystem::path{ FASTJUNGLE_DEFAULT_COOKED_DIR } / "JungleRuins.fjscene";
        fjr::render::RendererOptions renderer;
        std::optional<std::filesystem::path> benchmark_output;
        std::uint32_t benchmark_warmup_frames = 60;
        std::uint32_t benchmark_frames = 240;
    };
    */
    options.renderer.frame_entire_scene = false;
    options.renderer.lod_selection = 
        fjr::render::LodSelectionMode::FINEST;
    options.renderer.objects.other = true;
    options.renderer.objects.other_foliage = true;
    options.renderer.objects.pyramid_moss = true;
    options.renderer.objects.river_forest = false;
    options.renderer.objects.river_seedling = true;
    options.renderer.objects.terrain = true;
    options.renderer.vsync = false;

    HWND hwnd = create_window(instance, 1280, 720);
    fjr::log::Logger::g_logger << fjr::log::asrt(hwnd != nullptr);
    MSVC_ASSUME(hwnd != nullptr);

    RECT rect{};
    auto res_rect = GetClientRect(hwnd, &rect);
    fjr::log::Logger::g_logger << fjr::log::asrt(res_rect);

    const uint32_t width = static_cast<uint32_t>(rect.right - rect.left);
    const uint32_t height = static_cast<uint32_t>(rect.bottom - rect.top);

    SetWindowTextW(hwnd, L"Fast Jungle Renderer");

    const auto scene = fjr::scene::StaticSceneReader::load(options.scene);
    g_application.init(hwnd, width, height, *scene, options.renderer);

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    auto frame_times = g_application.run([] {

        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT)
                return false;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return true;

        },
        options.benchmark_output
            ? options.benchmark_warmup_frames
            : 0,
        options.benchmark_output
            ? options.benchmark_frames
            : 0);

    if (options.benchmark_output) {
        write_benchmark(
            *options.benchmark_output,
            frame_times);
    }

    return 0;
}
