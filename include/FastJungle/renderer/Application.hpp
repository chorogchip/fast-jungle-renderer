#pragma once

#include "FastJungle/renderer/RendererMain.hpp"

#include <cstdint>

namespace fjr {

    struct RunLoop {
        void* context = nullptr;
        bool (*pump_messages)(void* context) = nullptr;
    };

    class Application {
    public:
        Application() = default;

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        Application(Application&&) = delete;
        Application& operator=(Application&&) = delete;

        void init(
            void* native_window,
            std::uint32_t width,
            std::uint32_t height,
            const scene::StaticScene& scene);

        [[nodiscard]]
        int run(const RunLoop& run_loop);

        void request_resize(
            std::uint32_t width,
            std::uint32_t height) noexcept;

        void set_render_path(
            RendererMain::RenderPath path) noexcept {
            renderer_.set_render_path(path);
        }

    private:
        bool minimized_ = false;

        RendererMain renderer_;
    };

} // namespace fjr
