#include "FastJungle/renderer/Camera.hpp"

#include <algorithm>
#include <cmath>

namespace fjr::render {

    namespace {

        constexpr float MIN_VERTICAL_FOV = 0.001f;
        constexpr float MAX_VERTICAL_FOV =
            DirectX::XM_PI - MIN_VERTICAL_FOV;

        constexpr float MAX_PITCH =
            DirectX::XM_PIDIV2 - 0.001f;

        constexpr float MIN_ASPECT_RATIO = 0.001f;
        constexpr float MIN_NEAR_PLANE = 0.0001f;
        constexpr float MIN_PLANE_DISTANCE = 0.0001f;
        constexpr float MIN_QUATERNION_LENGTH_SQ = 0.000001f;

    } // namespace

    Camera::Camera() noexcept {
        init(
            DirectX::XMFLOAT3{},
            DirectX::XMFLOAT4{
                0.0f,
                0.0f,
                0.0f,
                1.0f,
            },
            DirectX::XM_PIDIV4,
            1.0f,
            0.1f,
            100000.0f,
            1.0f,
            1.0f);
    }

    Camera::Camera(
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT4& rotation,
        float vertical_fov,
        float aspect_ratio,
        float near_plane,
        float far_plane,
        float move_speed,
        float rotate_speed) noexcept {
        init(
            position,
            rotation,
            vertical_fov,
            aspect_ratio,
            near_plane,
            far_plane,
            move_speed,
            rotate_speed);
    }

    void Camera::init(
        const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT4& rotation,
        float vertical_fov,
        float aspect_ratio,
        float near_plane,
        float far_plane,
        float move_speed,
        float rotate_speed) noexcept {
        position_ = position;

        vertical_fov_ = std::clamp(
            vertical_fov,
            MIN_VERTICAL_FOV,
            MAX_VERTICAL_FOV);

        aspect_ratio_ = std::max(
            aspect_ratio,
            MIN_ASPECT_RATIO);

        near_plane_ = std::max(
            near_plane,
            MIN_NEAR_PLANE);

        far_plane_ = std::max(
            far_plane,
            near_plane_ + MIN_PLANE_DISTANCE);

        move_speed_ = std::max(
            move_speed,
            0.0f);

        rotate_speed_ = std::max(
            rotate_speed,
            0.0f);

        set_rotation(rotation);
    }

    void Camera::set_position(
        const DirectX::XMFLOAT3& position) noexcept {
        position_ = position;
        calc_matrix();
    }

    void Camera::set_rotation(
        const DirectX::XMFLOAT4& rotation) noexcept {
        using namespace DirectX;

        XMVECTOR rotation_vec =
            XMLoadFloat4(&rotation);

        const float length_sq = XMVectorGetX(
            XMVector4LengthSq(rotation_vec));

        if (!std::isfinite(length_sq) ||
            length_sq < MIN_QUATERNION_LENGTH_SQ) {
            rotation_vec = XMQuaternionIdentity();
        } else {
            rotation_vec =
                XMQuaternionNormalize(rotation_vec);
        }

        XMStoreFloat4(
            &rotation_,
            rotation_vec);

        const XMVECTOR forward = XMVector3Rotate(
            XMVectorSet(
                0.0f,
                0.0f,
                1.0f,
                0.0f),
            rotation_vec);

        XMFLOAT3 forward_vec{};
        XMStoreFloat3(
            &forward_vec,
            forward);

        yaw_ = std::atan2(
            forward_vec.x,
            forward_vec.z);

        pitch_ = std::asin(
            std::clamp(
                forward_vec.y,
                -1.0f,
                1.0f));

        calc_matrix();
    }

    void Camera::set_aspect_ratio(float aspect_ratio) noexcept {
        aspect_ratio_ = std::max(
            aspect_ratio,
            MIN_ASPECT_RATIO);
        calc_matrix();
    }

    void Camera::move_up(float delta_time) noexcept {
        position_.y += move_speed_ * delta_time;
        calc_matrix();
    }

    void Camera::move_forward(float delta_time) noexcept {
        const float distance =
            move_speed_ * delta_time;

        position_.x += std::sin(yaw_) * distance;
        position_.z += std::cos(yaw_) * distance;

        calc_matrix();
    }

    void Camera::rotate_right(float delta_time) noexcept {
        yaw_ = std::remainder(
            yaw_ + rotate_speed_ * delta_time,
            DirectX::XM_2PI);

        DirectX::XMStoreFloat4(
            &rotation_,
            DirectX::XMQuaternionRotationRollPitchYaw(
                pitch_,
                yaw_,
                0.0f));

        calc_matrix();
    }

    void Camera::rotate_up(float delta_time) noexcept {
        constexpr float MAX_PITCH =
            DirectX::XM_PIDIV2 - 0.001f;

        pitch_ = std::clamp(
            pitch_ + rotate_speed_ * delta_time,
            -MAX_PITCH,
            MAX_PITCH);

        DirectX::XMStoreFloat4(
            &rotation_,
            DirectX::XMQuaternionRotationRollPitchYaw(
                pitch_,
                yaw_,
                0.0f));

        calc_matrix();
    }

    void Camera::calc_matrix() noexcept {
        using namespace DirectX;

        const XMVECTOR rotation =
            XMLoadFloat4(&rotation_);

        const XMMATRIX world =
            XMMatrixMultiply(
                XMMatrixRotationQuaternion(rotation),
                XMMatrixTranslation(
                    position_.x,
                    position_.y,
                    position_.z));

        const XMMATRIX view =
            XMMatrixInverse(
                nullptr,
                world);

        const XMMATRIX projection =
            XMMatrixPerspectiveFovLH(
                vertical_fov_,
                aspect_ratio_,
                near_plane_,
                far_plane_);

        XMStoreFloat4x4(
            &world_,
            world);

        XMStoreFloat4x4(
            &view_,
            view);

        XMStoreFloat4x4(
            &projection_,
            projection);

        XMStoreFloat4x4(
            &view_projection_,
            XMMatrixMultiply(
                view,
                projection));
    }

} // namespace fjr::render
