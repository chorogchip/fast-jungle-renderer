
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
#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/scene/StaticSceneReader.hpp"

namespace {

    fjr::Application* gp_application;

    LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {

        switch (message) {

        case WM_SIZE:
            gp_application->resize(
                static_cast<UINT>(LOWORD(lparam)),
                static_cast<UINT>(HIWORD(lparam)));
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

    HWND hwnd = create_window(instance, 1920, 1080);
    fjr::log::Logger::g_logger << fjr::log::asrt(hwnd != nullptr);
    MSVC_ASSUME(hwnd != nullptr);

    RECT rect{};
    auto res_rect = GetClientRect(hwnd, &rect);
    fjr::log::Logger::g_logger << fjr::log::asrt(res_rect);

    const uint32_t width = static_cast<uint32_t>(rect.right - rect.left);
    const uint32_t height = static_cast<uint32_t>(rect.bottom - rect.top);

    SetWindowTextW(hwnd, L"Fast Jungle Renderer");

    std::filesystem::path scene_path = std::filesystem::path{ FASTJUNGLE_DEFAULT_COOKED_DIR } / "JungleRuins.fjscene";

    auto app = std::make_unique<fjr::Application>();
    gp_application = app.get();

    auto scene = fjr::scene::StaticSceneReader::load(scene_path);


    app->init(hwnd, width, height, *scene);

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    app->run([] {

        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT)
                return false;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return true;

        });

    gp_application = nullptr;
    app.reset();

    return 0;
}
