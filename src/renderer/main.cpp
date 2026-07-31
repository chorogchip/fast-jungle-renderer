#include <Windows.h>
#include <objbase.h>

#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>

namespace {

    constexpr wchar_t kWindowClassName[] = L"FastJungleRendererWindow";
    constexpr wchar_t kWindowTitle[] = L"FastJungle Renderer";
    constexpr int kInitialClientWidth = 1600;
    constexpr int kInitialClientHeight = 900;

    struct RendererState {
        bool initialized = false;
        UINT width = 0;
        UINT height = 0;
        bool minimized = false;
    };

    [[nodiscard]] RendererState* getRendererState(HWND window) noexcept {
        return reinterpret_cast<RendererState*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    void resizeRenderer(RendererState& state, UINT width, UINT height) {
        state.width = width;
        state.height = height;
        state.minimized = width == 0 || height == 0;

        if (!state.initialized || state.minimized) {
            return;
        }

        // Resize the DXGI swap chain and recreate RTV/DSV resources here.
    }

    [[nodiscard]] bool initializeRenderer(
        RendererState& state,
        HWND window,
        UINT width,
        UINT height) {

        UNREFERENCED_PARAMETER(window);

        state.width = width;
        state.height = height;

        // Initialize the runtime-only pipeline here:
        //
        // 1. Create the DXGI factory and select an adapter.
        // 2. Create the D3D12 device, queues, fences, and descriptor heaps.
        // 3. Create the swap chain for 'window'.
        // 4. Load the cooked .fjscene binary from assets/cooked.
        // 5. Create GPU buffers, textures, pipelines, and root signatures.
        // 6. Load the build-time DXIL files from the configured shader directory.

        state.initialized = true;
        return true;
    }

    void renderFrame(RendererState& state) {
        if (!state.initialized || state.minimized) {
            return;
        }

        // Record and submit the current frame, present the swap chain,
        // and advance per-frame synchronization here.
    }

    void shutdownRenderer(RendererState& state) noexcept {
        if (!state.initialized) {
            return;
        }

        // Wait for the GPU and release renderer resources here.
        state.initialized = false;
    }

    LRESULT CALLBACK windowProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam) {

        switch (message) {
        case WM_NCCREATE: {
            const auto* create =
                reinterpret_cast<const CREATESTRUCTW*>(lParam);
            auto* state =
                static_cast<RendererState*>(create->lpCreateParams);

            SetWindowLongPtrW(
                window,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(state));
            return TRUE;
        }

        case WM_SIZE:
            if (auto* state = getRendererState(window)) {
                resizeRenderer(
                    *state,
                    LOWORD(lParam),
                    HIWORD(lParam));
            }
            return 0;

        case WM_ERASEBKGND:
            // The renderer covers the full client area.
            return 1;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                PostMessageW(window, WM_CLOSE, 0, 0);
                return 0;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(window);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(EXIT_SUCCESS);
            return 0;

        case WM_NCDESTROY:
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            break;

        default:
            break;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }

    [[nodiscard]] ATOM registerWindowClass(HINSTANCE instance) {
        WNDCLASSEXW windowClass{
            .cbSize = sizeof(WNDCLASSEXW),
            .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
            .lpfnWndProc = windowProcedure,
            .cbClsExtra = 0,
            .cbWndExtra = 0,
            .hInstance = instance,
            .hIcon = LoadIconW(nullptr, IDI_APPLICATION),
            .hCursor = LoadCursorW(nullptr, IDC_ARROW),
            .hbrBackground = nullptr,
            .lpszMenuName = nullptr,
            .lpszClassName = kWindowClassName,
            .hIconSm = LoadIconW(nullptr, IDI_APPLICATION),
        };

        const ATOM atom = RegisterClassExW(&windowClass);
        if (atom == 0) {
            throw std::runtime_error("RegisterClassExW failed.");
        }

        return atom;
    }

    [[nodiscard]] HWND createMainWindow(
        HINSTANCE instance,
        RendererState& state,
        int showCommand) {

        RECT rectangle{
            .left = 0,
            .top = 0,
            .right = kInitialClientWidth,
            .bottom = kInitialClientHeight,
        };

        constexpr DWORD windowStyle = WS_OVERLAPPEDWINDOW;
        constexpr DWORD extendedStyle = 0;

        if (!AdjustWindowRectEx(
            &rectangle,
            windowStyle,
            FALSE,
            extendedStyle)) {
            throw std::runtime_error("AdjustWindowRectEx failed.");
        }

        HWND window = CreateWindowExW(
            extendedStyle,
            kWindowClassName,
            kWindowTitle,
            windowStyle,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rectangle.right - rectangle.left,
            rectangle.bottom - rectangle.top,
            nullptr,
            nullptr,
            instance,
            &state);

        if (!window) {
            throw std::runtime_error("CreateWindowExW failed.");
        }

        ShowWindow(window, showCommand);
        UpdateWindow(window);
        return window;
    }

    [[nodiscard]] int runMessageLoop(RendererState& state) {
        MSG message{};

        while (true) {
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) {
                    return static_cast<int>(message.wParam);
                }

                TranslateMessage(&message);
                DispatchMessageW(&message);
            }

            renderFrame(state);

            // Remove this wait once renderFrame blocks on the frame-latency
            // waitable object or another renderer synchronization primitive.
            MsgWaitForMultipleObjectsEx(
                0,
                nullptr,
                1,
                QS_ALLINPUT,
                MWMO_INPUTAVAILABLE);
        }
    }

} // namespace

int WINAPI wWinMain(
    _In_ HINSTANCE instance,
    _In_opt_ HINSTANCE previousInstance,
    _In_ PWSTR commandLine,
    _In_ int showCommand) {

    UNREFERENCED_PARAMETER(previousInstance);
    UNREFERENCED_PARAMETER(commandLine);

    SetProcessDpiAwarenessContext(
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    const HRESULT comResult = CoInitializeEx(
        nullptr,
        COINIT_MULTITHREADED);

    if (FAILED(comResult) &&
        comResult != RPC_E_CHANGED_MODE) {
        MessageBoxW(
            nullptr,
            L"COM initialization failed.",
            kWindowTitle,
            MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    const bool ownsComInitialization = SUCCEEDED(comResult);

    RendererState rendererState{};

    try {
        (void) registerWindowClass(instance);

        const HWND window =
            createMainWindow(instance, rendererState, showCommand);

        RECT clientRectangle{};
        if (!GetClientRect(window, &clientRectangle)) {
            throw std::runtime_error("GetClientRect failed.");
        }

        const UINT clientWidth = static_cast<UINT>(
            clientRectangle.right - clientRectangle.left);
        const UINT clientHeight = static_cast<UINT>(
            clientRectangle.bottom - clientRectangle.top);

        if (!initializeRenderer(
            rendererState,
            window,
            clientWidth,
            clientHeight)) {
            throw std::runtime_error(
                "Renderer initialization failed.");
        }

        const int exitCode = runMessageLoop(rendererState);
        shutdownRenderer(rendererState);

        if (ownsComInitialization) {
            CoUninitialize();
        }
        return exitCode;
    } catch (const std::exception& exception) {
        shutdownRenderer(rendererState);

        if (ownsComInitialization) {
            CoUninitialize();
        }

        const std::string message =
            std::string{ "Unhandled renderer exception:\n" } +
            exception.what();

        MessageBoxA(
            nullptr,
            message.c_str(),
            "FastJungle Renderer",
            MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    } catch (...) {
        shutdownRenderer(rendererState);

        if (ownsComInitialization) {
            CoUninitialize();
        }

        MessageBoxW(
            nullptr,
            L"Unhandled renderer exception.",
            kWindowTitle,
            MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }
}