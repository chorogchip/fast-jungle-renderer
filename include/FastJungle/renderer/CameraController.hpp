#pragma once

#include <chrono>

namespace fjr::render {

    class Camera;
    class CameraController {

    public:
        CameraController() = default;

        void bind(Camera* camera);
        void set_speed(float speed) { speed_ = speed; }
        void update();

    private:
        float speed_ = 1.0f;
        Camera* camera_ = nullptr;
        std::chrono::steady_clock::time_point previous_update_time_ =
            std::chrono::steady_clock::now();
    };

} // namespace fjr::render

