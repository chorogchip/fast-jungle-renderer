#pragma once

#include <cstdint>
#include <functional>

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

        void run(std::function<bool()> pump_messages);
        void resize(uint32_t width, uint32_t height);
        void handle_key_down(uint32_t virtual_key);

    private:
        render::RendererMain renderer_;
    };

} // namespace fjr
