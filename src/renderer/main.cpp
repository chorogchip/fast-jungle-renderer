
#include <Windows.h>
#include <shellapi.h>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include "FastJungle/core/util/Assume.h"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/renderer/Application.hpp"
#include "FastJungle/scene/StaticSceneReader.hpp"

namespace {

    fjr::Application g_application;

    struct RendererLaunchOptions {
        std::filesystem::path scene = std::filesystem::path{ FASTJUNGLE_DEFAULT_COOKED_DIR } / "JungleRuins.fjscene";
        fjr::render::RendererOptions renderer;
    };

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

    const auto options = get_renderer_launch_options();

    HWND hwnd = create_window(instance, 1280, 720);
    fjr::log::Logger::g_logger << fjr::log::asrt(hwnd != nullptr);
    MSVC_ASSUME(hwnd != nullptr);

    RECT rect{};
    auto res_rect = GetClientRect(hwnd, &rect);
    fjr::log::Logger::g_logger << fjr::log::asrt(res_rect);

    const auto width = rect.right - rect.left;
    const auto height = rect.bottom - rect.top;

    SetWindowTextW(hwnd, L"Fast Jungle Renderer");

    const auto scene = fjr::scene::StaticSceneReader::load(options.scene);
    g_application.init(hwnd, width, height, *scene, options.renderer);

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    g_application.run([] {

        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT)
                return false;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return true;

        });

    return 0;
}
