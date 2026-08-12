#include "FastJungle/renderer/Application.hpp"

#include <chrono>

#include "FastJungle/core/util/Logger.hpp"

namespace fjr {

    void Application::init(
        void* native_window,
        uint32_t width, uint32_t height,
        const scene::StaticScene& scene) {

        renderer_.init(
            native_window,
            width, height,
            scene);

        camera_controller_.bind(&renderer_.camera);
        camera_controller_.set_speed(1.0f);
    }

    void Application::run(std::function<bool()> pump_messages) {

        using Clock = std::chrono::steady_clock;

        while (pump_messages()) {
            const auto time_begin = Clock::now();

            camera_controller_.update();

            renderer_.render();
            const auto time_end = Clock::now();

            const double frame_time_ms =
                std::chrono::duration<double, std::milli>(
                    time_end - time_begin).count();

            fjr::log::Logger::g_logger_debug_out <<
                "Frame Time: [" << frame_time_ms << " ms]\n";
            fjr::log::Logger::g_logger_debug_out.flush_debug_string();
        }

        renderer_.reset();
    }

    void Application::resize(uint32_t width, uint32_t height) {
        if (width != 0 && height != 0)
            renderer_.resize(width, height);
    }

} // namespace fjr
