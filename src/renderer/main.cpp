#include "FastJungle/renderer/Application.hpp"
#include "FastJungle/scene/StaticSceneSaver.hpp"

#include <Windows.h>
#include <shellapi.h>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace {

    std::optional<int> run_scene_verification_mode() noexcept {
        int argc = 0;
        wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv == nullptr) {
            return EXIT_FAILURE;
        }

        const bool requested = argc >= 2 &&
            std::wstring_view{argv[1]} == L"--verify-scene";
        if (!requested) {
            LocalFree(argv);
            return std::nullopt;
        }

        if (argc != 3) {
            OutputDebugStringA(
                "Usage: FastJungle.exe --verify-scene input.fjscene\n");
            LocalFree(argv);
            return EXIT_FAILURE;
        }

        const std::filesystem::path path{argv[2]};
        LocalFree(argv);

        try {
            const auto scene = fjr::scene::StaticSceneSaver::load(path);
            const std::string message =
                "FastJungle renderer read and validated StaticScene: " +
                path.generic_string() + "\n";
            OutputDebugStringA(message.c_str());
            return EXIT_SUCCESS;
        }
        catch (const std::exception& exception) {
            const std::string message =
                "FastJungle renderer scene verification failed: " +
                std::string{exception.what()} + "\n";
            OutputDebugStringA(message.c_str());
            return EXIT_FAILURE;
        }
    }

    struct Win32State {
        fjr::Application* application = nullptr;
        bool minimized = false;
    };

    LRESULT CALLBACK window_proc(
        HWND hwnd,
        UINT message,
        WPARAM wparam,
        LPARAM lparam) {

        auto* state =
            reinterpret_cast<Win32State*>(
                GetWindowLongPtrW(
                    hwnd,
                    GWLP_USERDATA));

        switch (message) {
        case WM_NCCREATE: {
            const auto* create =
                reinterpret_cast<const CREATESTRUCTW*>(
                    lparam);

            state =
                static_cast<Win32State*>(
                    create->lpCreateParams);

            SetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(state));

            return TRUE;
        }

        case WM_SIZE:
            if (state != nullptr) {
                state->minimized =
                    wparam == SIZE_MINIMIZED;

                if (state->application != nullptr) {
                    state->application->request_resize(
                        static_cast<UINT>(LOWORD(lparam)),
                        static_cast<UINT>(HIWORD(lparam)));
                }
            }

            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(
                hwnd,
                message,
                wparam,
                lparam);
        }
    }

    bool pump_messages(void* context) {
        auto* state =
            static_cast<Win32State*>(context);

        MSG message{};

        while (PeekMessageW(
            &message,
            nullptr,
            0,
            0,
            PM_REMOVE)) {

            if (message.message == WM_QUIT) {
                return false;
            }

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (state->minimized) {
            WaitMessage();
        }

        return true;
    }

    HWND create_window(
        HINSTANCE instance,
        Win32State* state,
        UINT width,
        UINT height) {

        constexpr wchar_t class_name[] =
            L"FastJungleWindow";

        WNDCLASSEXW window_class{};
        window_class.cbSize =
            sizeof(window_class);
        window_class.style =
            CS_HREDRAW | CS_VREDRAW;
        window_class.lpfnWndProc =
            window_proc;
        window_class.hInstance =
            instance;
        window_class.hCursor =
            LoadCursorW(nullptr, IDC_ARROW);
        window_class.lpszClassName =
            class_name;

        if (RegisterClassExW(&window_class) == 0) {
            return nullptr;
        }

        RECT rectangle{
            0,
            0,
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
            state);
    }

} // namespace

int WINAPI wWinMain(
    _In_ HINSTANCE instance,
    _In_opt_ HINSTANCE,
    _In_ PWSTR,
    _In_ int show_command) {
    if (const auto result = run_scene_verification_mode()) {
        return *result;
    }

    constexpr UINT initial_width = 1280;
    constexpr UINT initial_height = 720;

    fjr::Application application;

    Win32State state{
        .application = nullptr,
        .minimized = false
    };

    HWND hwnd = create_window(
        instance,
        &state,
        initial_width,
        initial_height);

    if (hwnd == nullptr) {
        return 1;
    }

    RECT client_rectangle{};

    if (!GetClientRect(
        hwnd,
        &client_rectangle)) {
        return 1;
    }

    const UINT client_width =
        static_cast<UINT>(
            client_rectangle.right -
            client_rectangle.left);

    const UINT client_height =
        static_cast<UINT>(
            client_rectangle.bottom -
            client_rectangle.top);

    SetWindowTextW(hwnd, L"Fast Jungle Renderer");

    try {
        const auto cooked_scene =
            std::filesystem::path{FASTJUNGLE_DEFAULT_COOKED_DIR} /
            "JungleRuins.fjscene";
        const auto scene =
            fjr::scene::StaticSceneSaver::load(cooked_scene);
        application.init(
            hwnd,
            client_width,
            client_height,
            *scene);
    }
    catch (const std::exception& exception) {
        MessageBoxA(
            nullptr,
            exception.what(),
            "Fast Jungle",
            MB_OK | MB_ICONERROR);
        DestroyWindow(hwnd);
        return 1;
    }

    state.application = &application;

    ShowWindow(
        hwnd,
        show_command);

    UpdateWindow(hwnd);

    const fjr::RunLoop run_loop{
        .context = &state,
        .pump_messages = pump_messages
    };

    const int exit_code = application.run(run_loop);
    // The window procedure must not retain the stack object past run().
    state.application = nullptr;
    return exit_code;
}
