#pragma once

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
            std::uint32_t height);

        [[nodiscard]]
        int run(const RunLoop& run_loop);

        void request_resize(
            std::uint32_t width,
            std::uint32_t height) noexcept;

    private:
        void resize(
            std::uint32_t width,
            std::uint32_t height);

        void update();
        void render();

        void* native_window_ = nullptr;

        std::uint32_t width_ = 0;
        std::uint32_t height_ = 0;

        std::uint32_t pending_width_ = 0;
        std::uint32_t pending_height_ = 0;

        bool resize_pending_ = false;
        bool minimized_ = false;
    };

} // namespace fjr