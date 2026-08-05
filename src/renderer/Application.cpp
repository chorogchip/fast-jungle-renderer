#include "FastJungle/renderer/Application.hpp"

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
        while (pump_messages())
            renderer_.render();
    }

    void Application::resize(uint32_t width, uint32_t height) {
        if (width != 0 && height != 0)
            renderer_.resize(width, height);
    }

    void Application::handle_key_down(uint32_t virtual_key) {
        renderer_.handle_key_down(virtual_key);
    }

} // namespace fjr
