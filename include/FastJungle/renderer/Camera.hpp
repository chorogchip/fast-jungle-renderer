#pragma once

#include <cmath>
#include <DirectXMath.h>

#include "FastJungle/core/math/AABB.hpp"
#include "FastJungle/core/math/Frustum.hpp"

namespace fjr::render {

    class Camera {
    public:
        Camera() noexcept;

        Camera(
            const DirectX::XMFLOAT3& position,
            const DirectX::XMFLOAT4& rotation,
            float vertical_fov,
            float aspect_ratio,
            float near_plane,
            float far_plane,
            float move_speed,
            float rotate_speed) noexcept;

        void init(
            const DirectX::XMFLOAT3& position,
            const DirectX::XMFLOAT4& rotation,
            float vertical_fov,
            float aspect_ratio,
            float near_plane,
            float far_plane,
            float move_speed,
            float rotate_speed) noexcept;

        void frame_at(const math::AABB& bounds) noexcept;

        void set_position(
            const DirectX::XMFLOAT3& position) noexcept;

        void set_rotation(
            const DirectX::XMFLOAT4& rotation) noexcept;

        void set_aspect_ratio(float aspect_ratio) noexcept;

        void move_up(float delta_time) noexcept;
        void move_forward(float delta_time) noexcept;
        void move_right(float delta_time) noexcept;
        void rotate_right(float delta_time) noexcept;
        void rotate_up(float delta_time) noexcept;

        [[nodiscard]]
        const DirectX::XMFLOAT3& get_position() const noexcept {
            return position_;
        }

        [[nodiscard]]
        const DirectX::XMFLOAT4& get_rotation() const noexcept {
            return rotation_;
        }

        [[nodiscard]]
        const DirectX::XMFLOAT4X4& get_world_mat() const noexcept {
            return world_;
        }

        [[nodiscard]]
        const DirectX::XMFLOAT4X4& get_view_mat() const noexcept {
            return view_;
        }

        [[nodiscard]]
        const DirectX::XMFLOAT4X4& get_projection_mat() const noexcept {
            return projection_;
        }

        [[nodiscard]]
        const DirectX::XMFLOAT4X4&
            get_view_projection_mat() const noexcept {
            return view_projection_;
        }

        [[nodiscard]]
        math::Frustum make_frustum() const noexcept;

    private:
        void calc_matrix() noexcept;

        DirectX::XMFLOAT3 position_{};
        DirectX::XMFLOAT4 rotation_{
            0.0f,
            0.0f,
            0.0f,
            1.0f,
        };

        float yaw_ = 0.0f;
        float pitch_ = 0.0f;

        float vertical_fov_ = DirectX::XM_PIDIV4;
        float aspect_ratio_ = 1.0f;
        float near_plane_ = 0.1f;
        float far_plane_ = 100000.0f;

        float move_speed_ = 1.0f;
        float rotate_speed_ = 1.0f;

        DirectX::XMFLOAT4X4 world_{};
        DirectX::XMFLOAT4X4 view_{};
        DirectX::XMFLOAT4X4 projection_{};
        DirectX::XMFLOAT4X4 view_projection_{};
    };

} // namespace fjr::render
