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
            const scene::StaticScene& scene,
            const render::RendererOptions& options = {});

        [[nodiscard]]
        int run(const RunLoop& run_loop);

        void request_resize(
            std::uint32_t width,
            std::uint32_t height) noexcept;

        void handle_key_down(std::uint32_t virtual_key) noexcept;

    private:
        bool minimized_ = false;

        render::RendererMain renderer_;
    };

} // namespace fjr
