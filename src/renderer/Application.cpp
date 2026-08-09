#include "FastJungle/renderer/Application.hpp"

#include <chrono>

#include "FastJungle/core/util/Logger.hpp"

namespace fjr {

    namespace {

        double calc_ema(double* ema, double sample) {
            constexpr double rise_alpha = 1.0;
            constexpr double fall_alpha = 0.02;

            const double alpha =
                sample > *ema ? rise_alpha : fall_alpha;

            *ema += alpha * (sample - *ema);
            return *ema;
        }
    }

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
            renderer_.render();
            const auto time_end = Clock::now();

            const double frame_time_ms =
                std::chrono::duration<double, std::milli>(
                    time_end - time_begin).count();

            fjr::log::Logger::g_logger_debug_out <<
                "Frame Time: [" << frame_time_ms <<
                " ms] EMA: [" <<
                calc_ema(&frame_time_ema_, frame_time_ms) <<
                " ms]\n";
            fjr::log::Logger::g_logger_debug_out.flush_debug_string();
        }

        renderer_.reset();
    }

    void Application::resize(uint32_t width, uint32_t height) {
        if (width != 0 && height != 0)
            renderer_.resize(width, height);
    }

    void Application::handle_key_down(uint32_t virtual_key) {
        camera_controller_.move(virtual_key);
    }

} // namespace fjr
