#include "CameraController.hpp"

#include "FastJungle/renderer/component/Camera.hpp"

#include <Windows.h>

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>
#include <cwchar>

namespace fjr::render::internal {

    namespace {

        constexpr float WALK_SPEED_METERS_PER_SECOND = 180.0f;
        constexpr float SPRINT_SPEED_METERS_PER_SECOND = 900.0f;
        constexpr float LOOK_SPEED_RADIANS_PER_SECOND = 1.4f;
        constexpr float TAP_MOVE_METERS = 12.0f;
        constexpr float TAP_LOOK_RADIANS = 0.08f;
        constexpr float MAX_FRAME_SECONDS = 0.1f;
        constexpr float MIN_FORWARD_UP_DOT = 0.985f;

        [[nodiscard]] bool key_down(int key) noexcept {
            return (GetAsyncKeyState(key) & 0x8000) != 0;
        }

        [[nodiscard]] float axis(int positive, int negative) noexcept {
            return static_cast<float>(key_down(positive)) -
                static_cast<float>(key_down(negative));
        }

    } // namespace

    CameraController::CameraController(void* native_window) noexcept
        : native_window_(native_window),
          previous_time_(std::chrono::steady_clock::now()) {}

    bool CameraController::update(Camera& camera) noexcept {
        const auto now = std::chrono::steady_clock::now();
        const float elapsed = std::clamp(
            std::chrono::duration<float>(now - previous_time_).count(),
            0.0f,
            MAX_FRAME_SECONDS);
        previous_time_ = now;

        const auto window = static_cast<HWND>(native_window_);
        if (elapsed <= 0.0f || window == nullptr ||
            GetForegroundWindow() != window) {
            return false;
        }

        const float speed = key_down(VK_SHIFT)
            ? SPRINT_SPEED_METERS_PER_SECOND
            : WALK_SPEED_METERS_PER_SECOND;
        return apply(
            camera,
            axis('D', 'A'),
            axis('E', 'Q'),
            axis('W', 'S'),
            axis(VK_RIGHT, VK_LEFT) *
                LOOK_SPEED_RADIANS_PER_SECOND * elapsed,
            axis(VK_UP, VK_DOWN) *
                LOOK_SPEED_RADIANS_PER_SECOND * elapsed,
            speed * elapsed);
    }

    bool CameraController::step(
        Camera& camera,
        std::uint32_t virtual_key,
        LodSelectionMode lod_selection) noexcept {

        float strafe = 0.0f;
        float lift = 0.0f;
        float advance = 0.0f;
        float yaw = 0.0f;
        float pitch = 0.0f;
        switch (virtual_key) {
        case 'A': strafe = -1.0f; break;
        case 'D': strafe = 1.0f; break;
        case 'Q': lift = -1.0f; break;
        case 'E': lift = 1.0f; break;
        case 'S': advance = -1.0f; break;
        case 'W': advance = 1.0f; break;
        case VK_LEFT: yaw = -TAP_LOOK_RADIANS; break;
        case VK_RIGHT: yaw = TAP_LOOK_RADIANS; break;
        case VK_DOWN: pitch = -TAP_LOOK_RADIANS; break;
        case VK_UP: pitch = TAP_LOOK_RADIANS; break;
        default: return false;
        }

        const bool changed = apply(
            camera,
            strafe,
            lift,
            advance,
            yaw,
            pitch,
            TAP_MOVE_METERS);
        if (changed) {
            update_caption(camera, lod_selection);
        }
        return changed;
    }

    bool CameraController::apply(
        Camera& camera,
        float strafe,
        float lift,
        float advance,
        float yaw,
        float pitch,
        float move_distance) noexcept {

        if (strafe == 0.0f && lift == 0.0f && advance == 0.0f &&
            yaw == 0.0f && pitch == 0.0f) {
            return false;
        }

        using namespace DirectX;

        XMMATRIX world = XMLoadFloat4x4(&camera.get_world());
        const XMVECTOR world_up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMVECTOR forward = XMVector3Normalize(world.r[2]);
        if (yaw != 0.0f) {
            forward = XMVector3Rotate(
                forward,
                XMQuaternionRotationAxis(world_up, yaw));
        }

        XMVECTOR right = XMVector3Normalize(
            XMVector3Cross(world_up, forward));
        if (pitch != 0.0f) {
            const XMVECTOR candidate = XMVector3Normalize(
                XMVector3Rotate(
                    forward,
                    XMQuaternionRotationAxis(right, pitch)));
            const float vertical = std::abs(
                XMVectorGetX(XMVector3Dot(candidate, world_up)));
            if (vertical < MIN_FORWARD_UP_DOT) {
                forward = candidate;
            }
        }

        right = XMVector3Normalize(XMVector3Cross(world_up, forward));
        const XMVECTOR up = XMVector3Normalize(
            XMVector3Cross(forward, right));

        XMVECTOR movement = XMVectorAdd(
            XMVectorScale(right, strafe),
            XMVectorAdd(
                XMVectorScale(world_up, lift),
                XMVectorScale(forward, advance)));
        if (XMVectorGetX(XMVector3LengthSq(movement)) > 1.0f) {
            movement = XMVector3Normalize(movement);
        }
        const XMVECTOR position = XMVectorAdd(
            world.r[3],
            XMVectorScale(movement, move_distance));

        world.r[0] = XMVectorSetW(right, 0.0f);
        world.r[1] = XMVectorSetW(up, 0.0f);
        world.r[2] = XMVectorSetW(forward, 0.0f);
        world.r[3] = XMVectorSetW(position, 1.0f);

        DirectX::XMFLOAT4X4 transform;
        XMStoreFloat4x4(&transform, world);
        camera.set_world_transform(transform);
        return true;
    }

    void CameraController::update_caption(
        const Camera& camera,
        LodSelectionMode lod_selection) const noexcept {

        const auto position = camera.get_world_position();
        const wchar_t* lod_label = L"Auto LOD";
        if (lod_selection == LodSelectionMode::FINEST) {
            lod_label = L"LOD Finest";
        } else if (lod_selection == LodSelectionMode::COARSEST) {
            lod_label = L"LOD Coarsest";
        }
        wchar_t title[192]{};
        swprintf_s(
            title,
            L"Fast Jungle Renderer - %s - Camera %.1f, %.1f, %.1f",
            lod_label,
            position.x,
            position.y,
            position.z);
        SetWindowTextW(static_cast<HWND>(native_window_), title);
    }

} // namespace fjr::render::internal
