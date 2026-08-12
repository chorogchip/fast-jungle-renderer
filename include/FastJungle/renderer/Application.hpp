#pragma once

#include <cstdint>
#include <functional>

#include "FastJungle/renderer/CameraController.hpp"
#include "FastJungle/renderer/RendererJungle.hpp"

namespace fjr {

    class Application {

    public:
        Application() = default;
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;
        Application(Application&&) = delete;
        Application& operator=(Application&&) = delete;

        void init(
            void* native_window,
            uint32_t width, uint32_t height,
            const scene::StaticScene& scene);

        void run(std::function<bool()> pump_messages);

        void resize(uint32_t width, uint32_t height);

    private:
        render::CameraController camera_controller_;
        render::RendererJungle renderer_;
    };

} // namespace fjr
