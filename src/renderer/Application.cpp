#include "FastJungle/renderer/Application.hpp"

#include "FastJungle/core/util/Logger.hpp"

namespace fjr {

    void Application::init(
        void* native_window,
        std::uint32_t width,
        std::uint32_t height,
        const scene::StaticScene& scene,
        const render::RendererOptions& options) {

        if (native_window == nullptr ||
            width == 0 ||
            height == 0) {
            log::Logger::g_logger
                << "Renderer application requires a valid window and size."
                << log::abrt();
        }

        minimized_ = false;

        renderer_.init(
            native_window,
            width,
            height,
            scene,
            options);
    }

    int Application::run(
        const RunLoop& run_loop) {

        if (run_loop.pump_messages == nullptr) {
            return 1;
        }

        while (run_loop.pump_messages(
            run_loop.context)) {

            if (minimized_) {
                continue;
            }

            renderer_.render();
        }

        return 0;
    }

    void Application::request_resize(
        std::uint32_t width,
        std::uint32_t height) noexcept {

        minimized_ = width == 0 || height == 0;
        if (!minimized_) {
            renderer_.resize(width, height);
        }
    }

    void Application::handle_key_down(
        std::uint32_t virtual_key) noexcept {
        renderer_.handle_key_down(virtual_key);
    }

} // namespace fjr
