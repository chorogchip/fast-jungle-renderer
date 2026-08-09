#include "FastJungle/renderer/CameraController.hpp"

#include <Windows.h>
#include <cmath>

#include "FastJungle/renderer/Camera.hpp"


namespace fjr::render {

    void CameraController::update(float delta_seconds) {
        if (camera_ == nullptr) {
            return;
        }

        const float base_step = std::abs(speed_) * delta_seconds;
        const float move_step = base_step * 5.0f;
        const float rotate_step = base_step * 10.0f;
        if ((GetAsyncKeyState('W') & 0x8000) != 0) {
            camera_->move_forward(move_step);
        }
        if ((GetAsyncKeyState('S') & 0x8000) != 0) {
            camera_->move_forward(-move_step);
        }
        if ((GetAsyncKeyState('A') & 0x8000) != 0) {
            camera_->move_right(-move_step);
        }
        if ((GetAsyncKeyState('D') & 0x8000) != 0) {
            camera_->move_right(move_step);
        }
        if ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0) {
            camera_->move_up(move_step);
        }
        if ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) {
            camera_->move_up(-move_step);
        }
        if ((GetAsyncKeyState(VK_RIGHT) & 0x8000) != 0) {
            camera_->rotate_right(rotate_step);
        }
        if ((GetAsyncKeyState(VK_LEFT) & 0x8000) != 0) {
            camera_->rotate_right(-rotate_step);
        }
        if ((GetAsyncKeyState(VK_UP) & 0x8000) != 0) {
            camera_->rotate_up(rotate_step);
        }
        if ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0) {
            camera_->rotate_up(-rotate_step);
        }
    }

} // namespace fjr::render
