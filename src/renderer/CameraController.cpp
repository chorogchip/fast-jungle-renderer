#include "FastJungle/renderer/CameraController.hpp"

#include <Windows.h>
#include <cmath>

#include "FastJungle/renderer/Camera.hpp"


namespace fjr::render {

    void CameraController::move(
        std::uint32_t virtual_key) {
        if (camera_ == nullptr) {
            return;
        }

        const float speed = std::abs(speed_);

        switch (virtual_key) {
        case 'W':
            camera_->move_forward(speed);
            break;

        case 'S':
            camera_->move_forward(-speed);
            break;

        case VK_SPACE:
            camera_->move_up(speed);
            break;

        case VK_LSHIFT:
            camera_->move_up(-speed);
            break;

        case VK_RIGHT:
            camera_->rotate_right(speed);
            break;

        case VK_LEFT:
            camera_->rotate_right(-speed);
            break;

        case VK_UP:
            camera_->rotate_up(speed);
            break;

        case VK_DOWN:
            camera_->rotate_up(-speed);
            break;

        default:
            break;
        }
    }

} // namespace fjr::render
