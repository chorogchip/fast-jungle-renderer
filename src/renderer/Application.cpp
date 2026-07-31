#include "FastJungle/renderer/Application.hpp"

#include <cstdlib>

namespace fjr {

    void Application::init(
        void* native_window,
        std::uint32_t width,
        std::uint32_t height) {

        if (native_window == nullptr ||
            width == 0 ||
            height == 0) {
            std::abort();
        }

        native_window_ = native_window;

        width_ = width;
        height_ = height;

        pending_width_ = width;
        pending_height_ = height;

        // renderer_.init(
        //     native_window_,
        //     width_,
        //     height_);
    }

    int Application::run(
        const RunLoop& run_loop) {

        if (run_loop.pump_messages == nullptr) {
            return 1;
        }

        while (run_loop.pump_messages(
            run_loop.context)) {

            if (resize_pending_) {
                resize(
                    pending_width_,
                    pending_height_);

                resize_pending_ = false;
            }

            if (minimized_) {
                continue;
            }

            update();
            render();
        }

        // renderer_.flush();

        return 0;
    }

    void Application::request_resize(
        std::uint32_t width,
        std::uint32_t height) noexcept {

        pending_width_ = width;
        pending_height_ = height;
        resize_pending_ = true;
    }

    void Application::resize(
        std::uint32_t width,
        std::uint32_t height) {

        if (width == 0 || height == 0) {
            minimized_ = true;
            return;
        }

        minimized_ = false;

        if (width_ == width &&
            height_ == height) {
            return;
        }

        width_ = width;
        height_ = height;

        // renderer_.resize(
        //     width_,
        //     height_);
    }

    void Application::update() {
        // 게임 및 애플리케이션 로직
    }

    void Application::render() {
        // renderer_.render();
    }

} // namespace fjr