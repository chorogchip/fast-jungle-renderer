#include "FastJungle/renderer/Application.hpp"
#include "FastJungle/scene/StaticSceneReader.hpp"

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

namespace {

    struct RendererLaunchOptions {
        std::filesystem::path scene =
            std::filesystem::path{FASTJUNGLE_DEFAULT_COOKED_DIR} /
            "JungleRuins.fjscene";
        std::optional<std::uint32_t> smoke_test_frames;
        fjr::render::RendererOptions renderer;
    };

    void write_smoke_error(
        const RendererLaunchOptions& options,
        std::string_view message) noexcept {

        if (!options.smoke_test_frames) {
            return;
        }
        std::ofstream{
            std::filesystem::temp_directory_path() /
            "FastJungle-smoke-error.txt",
            std::ios::trunc}
            << message;
    }

    RendererLaunchOptions get_renderer_launch_options() noexcept {
        RendererLaunchOptions options;

        std::error_code path_error;
        if (!std::filesystem::exists(options.scene, path_error)) {
            const auto bc_scene =
                std::filesystem::path{FASTJUNGLE_DEFAULT_COOKED_DIR} /
                "JungleRuins-release-bc.fjscene";
            if (std::filesystem::exists(bc_scene, path_error)) {
                options.scene = bc_scene;
            }
        }

        int argc = 0;
        wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv == nullptr) {
            return options;
        }

        for (int index = 1; index < argc; ++index) {
            const std::wstring_view argument{argv[index]};
            if (argument == L"--scene" && index + 1 < argc) {
                options.scene = argv[++index];
            } else if (
                argument == L"--smoke-test-frames" &&
                index + 1 < argc) {
                options.smoke_test_frames =
                    static_cast<std::uint32_t>(
                        std::wcstoul(argv[++index], nullptr, 10));
            } else if (argument == L"--force-lod0") {
                options.renderer.lod_selection =
                    fjr::render::LodSelectionMode::FINEST;
            } else if (argument == L"--force-coarsest-lod") {
                options.renderer.lod_selection =
                    fjr::render::LodSelectionMode::COARSEST;
            } else if (argument == L"--overview") {
                options.renderer.frame_entire_scene = true;
            }
        }

        LocalFree(argv);
        return options;
    }

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
            const auto scene = fjr::scene::StaticSceneReader::load(path);
            
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
        std::optional<std::uint32_t> smoke_test_frames;
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

        case WM_KEYDOWN:
            if (state != nullptr && state->application != nullptr &&
                (lparam & (1ll << 30)) == 0) {
                state->application->handle_key_down(
                    static_cast<std::uint32_t>(wparam));
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

        if (state->smoke_test_frames) {
            if (*state->smoke_test_frames == 0) {
                return false;
            }
            --*state->smoke_test_frames;
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
    const RendererLaunchOptions launch_options =
        get_renderer_launch_options();

    Win32State state{
        .application = nullptr,
        .minimized = false,
        .smoke_test_frames = launch_options.smoke_test_frames,
    };

    HWND hwnd = create_window(
        instance,
        &state,
        initial_width,
        initial_height);

    if (hwnd == nullptr) {
        write_smoke_error(
            launch_options,
            "Unable to create renderer window (Win32 error " +
                std::to_string(GetLastError()) + ").");
        return 1;
    }

    RECT client_rectangle{};

    if (!GetClientRect(
        hwnd,
        &client_rectangle)) {
        write_smoke_error(
            launch_options,
            "Unable to read renderer client area (Win32 error " +
                std::to_string(GetLastError()) + ").");
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

    std::wstring window_title = L"Fast Jungle Renderer - ";
    switch (launch_options.renderer.lod_selection) {
    case fjr::render::LodSelectionMode::FINEST:
        window_title += L"LOD Finest";
        break;
    case fjr::render::LodSelectionMode::COARSEST:
        window_title += L"LOD Coarsest";
        break;
    default:
        window_title += L"Auto LOD";
        break;
    }
    if (launch_options.renderer.frame_entire_scene) {
        window_title += L" - Overview";
    }
    window_title += L" - WASD/QE + Arrow Keys";
    SetWindowTextW(hwnd, window_title.c_str());

    try {

        const auto scene = fjr::scene::StaticSceneReader::load(launch_options.scene);
        application.init(
            hwnd,
            client_width,
            client_height,
            *scene,
            launch_options.renderer);
    }
    catch (const std::exception& exception) {
        if (launch_options.smoke_test_frames) {
            OutputDebugStringA(exception.what());
            write_smoke_error(launch_options, exception.what());
        } else {
            MessageBoxA(
                nullptr,
                exception.what(),
                "Fast Jungle",
                MB_OK | MB_ICONERROR);
        }
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

    int exit_code = EXIT_FAILURE;
    try {
        exit_code = application.run(run_loop);
    }
    catch (const std::exception& exception) {
        write_smoke_error(launch_options, exception.what());
        if (!launch_options.smoke_test_frames) {
            MessageBoxA(
                nullptr,
                exception.what(),
                "Fast Jungle",
                MB_OK | MB_ICONERROR);
        }
        DestroyWindow(hwnd);
    }
    // The window procedure must not retain the stack object past run().
    state.application = nullptr;
    return exit_code;
}
