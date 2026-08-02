#include "FastJungle/renderer/Application.hpp"

#include <cstdlib>

namespace fjr {

    void Application::init(
        void* native_window,
        std::uint32_t width,
        std::uint32_t height,
        const scene::JungleScene& scene) {

        if (native_window == nullptr ||
            width == 0 ||
            height == 0) {
            std::abort();
        }

        renderer_.init(
            native_window,
            width,
            height,
            scene);
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

        renderer_.close();

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

} // namespace fjr
