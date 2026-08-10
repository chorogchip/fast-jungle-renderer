#include "FastJungle/renderer/CameraController.hpp"

#include <Windows.h>
#include <cmath>

#include "FastJungle/renderer/Camera.hpp"


namespace fjr::render {

    void CameraController::update(float delta_seconds) {
        if (camera_ == nullptr) {
            return;
        }

        float speed = std::abs(speed_) * delta_seconds * 5.0f;
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
            speed *= 20.0f;

        if (GetAsyncKeyState('W') & 0x8000)
            camera_->move_forward(speed);
        
        if (GetAsyncKeyState('S') & 0x8000)
            camera_->move_forward(-speed);
        
        if (GetAsyncKeyState('A') & 0x8000)
            camera_->move_right(-speed);
        
        if (GetAsyncKeyState('D') & 0x8000)
            camera_->move_right(speed);
        
        if (GetAsyncKeyState(VK_SPACE) & 0x8000)
            camera_->move_up(speed);
        
        if (GetAsyncKeyState(VK_LSHIFT) & 0x8000)
            camera_->move_up(-speed);
        
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
            camera_->rotate_right(speed);
        
        if (GetAsyncKeyState(VK_LEFT) & 0x8000)
            camera_->rotate_right(-speed);
        
        if (GetAsyncKeyState(VK_UP) & 0x8000)
            camera_->rotate_up(speed);
        
        if (GetAsyncKeyState(VK_DOWN) & 0x8000)
            camera_->rotate_up(-speed);
    }

} // namespace fjr::render
