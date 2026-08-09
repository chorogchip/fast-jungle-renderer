#pragma once

namespace fjr::render {

    class Camera;
    class CameraController {

    public:
        CameraController() = default;

        void bind(Camera* camera) { camera_ = camera; }
        void set_speed(float speed) { speed_ = speed; }
        void update(float delta_seconds);

    private:
        float speed_ = 1.0f;
        Camera* camera_ = nullptr;
    };

} // namespace fjr::render

