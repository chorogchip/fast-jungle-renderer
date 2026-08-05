#pragma once

#include "FastJungle/renderer/RendererOptions.hpp"

#include <chrono>
#include <cstdint>

namespace fjr::render {
    class Camera;

    namespace internal {

        // Renderer-private Win32 input adapter. Camera remains platform neutral;
        // this class alone translates keyboard state into a camera pose.
        class CameraController final {
        public:
            explicit CameraController(void* native_window) noexcept;

            [[nodiscard]] bool update(Camera& camera) noexcept;
            [[nodiscard]] bool step(
                Camera& camera,
                std::uint32_t virtual_key,
                LodSelectionMode lod_selection) noexcept;

        private:
            [[nodiscard]] static bool apply(
                Camera& camera,
                float strafe,
                float lift,
                float advance,
                float yaw,
                float pitch,
                float move_distance) noexcept;

            void update_caption(
                const Camera& camera,
                LodSelectionMode lod_selection) const noexcept;

            void* native_window_ = nullptr;
            std::chrono::steady_clock::time_point previous_time_;
        };

    } // namespace internal
} // namespace fjr::render
