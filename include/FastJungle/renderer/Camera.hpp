#pragma once

#include "FastJungle/core/math/AABB.hpp"
#include "FastJungle/scene/StaticScene.hpp"

#include <DirectXMath.h>

#include <cstdint>

namespace fjr::render {

    class Camera {
    public:
        enum class ApertureFit : std::uint32_t {
            HORIZONTAL,
            VERTICAL,
            OVERSCAN,
            FILL,
        };

        Camera();
        explicit Camera(const scene::StaticScene::Camera& source);

        void set_scene_camera(
            const scene::StaticScene::Camera& source) noexcept;
        void set_world_transform(
            const DirectX::XMFLOAT4X4& world_transform) noexcept;
        void set_viewport(
            std::uint32_t width,
            std::uint32_t height) noexcept;
        void set_aperture_fit(ApertureFit fit) noexcept;

        // Replaces the pose and invalid lens values with a camera that frames
        // the supplied bounds. This is an explicit renderer fallback, not an
        // implicit correction of a valid scene camera.
        void frame_bounds(const math::AABB& bounds) noexcept;

        [[nodiscard]]
        bool has_valid_lens() const noexcept;

        [[nodiscard]]
        bool has_valid_transform() const noexcept {
            return valid_transform_;
        }

        [[nodiscard]]
        const scene::StaticScene::Camera& get_scene_camera() const noexcept {
            return source_;
        }

        [[nodiscard]]
        const DirectX::XMFLOAT4X4& get_world() const noexcept {
            return world_;
        }

        [[nodiscard]]
        const DirectX::XMFLOAT4X4& get_view() const noexcept {
            return view_;
        }

        [[nodiscard]]
        const DirectX::XMFLOAT4X4& get_projection() const noexcept {
            return projection_;
        }

        [[nodiscard]]
        const DirectX::XMFLOAT4X4& get_view_projection() const noexcept {
            return view_projection_;
        }

        [[nodiscard]]
        const DirectX::XMFLOAT3& get_world_position() const noexcept {
            return world_position_;
        }

        [[nodiscard]]
        float get_near_plane() const noexcept {
            return near_plane_;
        }

        [[nodiscard]]
        float get_far_plane() const noexcept {
            return far_plane_;
        }

        [[nodiscard]]
        float get_aspect_ratio() const noexcept;

    private:
        void update() noexcept;
        void update_view() noexcept;
        void update_projection() noexcept;

        scene::StaticScene::Camera source_{};

        std::uint32_t viewport_width_ = 1;
        std::uint32_t viewport_height_ = 1;
        ApertureFit aperture_fit_ = ApertureFit::OVERSCAN;

        DirectX::XMFLOAT4X4 world_ =
            scene::StaticScene::IDENTITY_TRANSFORM;
        DirectX::XMFLOAT4X4 view_ =
            scene::StaticScene::IDENTITY_TRANSFORM;
        DirectX::XMFLOAT4X4 projection_ =
            scene::StaticScene::IDENTITY_TRANSFORM;
        DirectX::XMFLOAT4X4 view_projection_ =
            scene::StaticScene::IDENTITY_TRANSFORM;
        DirectX::XMFLOAT3 world_position_{};

        float near_plane_ = 0.1f;
        float far_plane_ = 100000.0f;
        bool valid_transform_ = true;
    };

} // namespace fjr::render
