#include "FastJungle/renderer/Application.hpp"

#include <chrono>

#include "FastJungle/core/util/Logger.hpp"

namespace fjr {

    void Application::init(
        void* native_window, uint32_t width, uint32_t height,
        const scene::StaticScene& scene,
        const render::RendererOptions& options) {

        renderer_.init(
            native_window, width, height,
            scene, options);
    }

    void Application::run(std::function<bool()> pump_messages) {

        using Clock = std::chrono::steady_clock;

        while (!renderer_.to_close() && pump_messages()) {

            const auto time_begin = Clock::now();
            renderer_.render();
            const auto time_end = Clock::now();

            const double frame_time_ms = 
                std::chrono::duration<double, std::milli>(
                    time_end - time_begin).count();

            const double weight = 0.1;
            frame_time_ema_ =
                frame_time_ema_ * (1.0 - weight) +
                frame_time_ms * weight;

            fjr::log::Logger::g_logger_debug_out <<
                "Frame Time: [" << frame_time_ms <<
                " ms] EMA: [" << frame_time_ema_ << " ms]\n";
            fjr::log::Logger::g_logger_debug_out.flush_debug_string();
        }
    }

    void Application::resize(uint32_t width, uint32_t height) {
        if (width != 0 && height != 0)
            renderer_.resize(width, height);
    }

    void Application::handle_key_down(uint32_t virtual_key) {
        renderer_.handle_key_down(virtual_key);
    }

} // namespace fjr
