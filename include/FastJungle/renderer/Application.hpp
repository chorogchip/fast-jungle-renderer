#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "FastJungle/renderer/CameraController.hpp"
#include "FastJungle/renderer/RendererMain.hpp"

namespace fjr {

    class Application {

    public:
        Application() = default;
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
        Application(Application&&) = delete;
        Application& operator=(Application&&) = delete;

        void init(
            void* native_window, uint32_t width, uint32_t height,
            const scene::StaticScene& scene,
            const render::RendererOptions& options = {});

        std::vector<double> run(
            std::function<bool()> pump_messages,
            std::uint32_t warmup_frames = 0,
            std::uint32_t measured_frames = 0);
        void resize(uint32_t width, uint32_t height);
        void handle_key_down(uint32_t virtual_key);

    private:
        render::CameraController camera_controller_;
        render::RendererMain renderer_;
        double frame_time_ema_;
    };

} // namespace fjr
