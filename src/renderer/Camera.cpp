#include "FastJungle/renderer/Camera.hpp"

#include <algorithm>
#include <cmath>

namespace fjr::render {

    namespace {

        constexpr float DEFAULT_FOCAL_LENGTH = 50.0f;
        constexpr float DEFAULT_HORIZONTAL_APERTURE = 36.0f;
        constexpr float DEFAULT_VERTICAL_APERTURE = 24.0f;
        constexpr float DEFAULT_NEAR_PLANE = 0.1f;
        constexpr float DEFAULT_FAR_PLANE = 100000.0f;
        constexpr float MINIMUM_VALUE = 1.0e-6f;

        [[nodiscard]]
        bool positive_finite(float value) noexcept {
            return std::isfinite(value) && value > MINIMUM_VALUE;
        }

    } // namespace

    Camera::Camera() {
        update();
    }

    Camera::Camera(
        const scene::StaticScene::Camera& source)
        : source_(source) {
        update();
    }

    void Camera::set_scene_camera(
        const scene::StaticScene::Camera& source) noexcept {
        source_ = source;
        update();
    }

    void Camera::set_world_transform(
        const DirectX::XMFLOAT4X4& world_transform) noexcept {
        source_.world_transform = world_transform;
        update_view();

        DirectX::XMStoreFloat4x4(
            &view_projection_,
            DirectX::XMMatrixMultiply(
                DirectX::XMLoadFloat4x4(&view_),
                DirectX::XMLoadFloat4x4(&projection_)));
    }

    void Camera::set_viewport(
        std::uint32_t width,
        std::uint32_t height) noexcept {
        viewport_width_ = std::max(1u, width);
        viewport_height_ = std::max(1u, height);
        update_projection();

        DirectX::XMStoreFloat4x4(
            &view_projection_,
            DirectX::XMMatrixMultiply(
                DirectX::XMLoadFloat4x4(&view_),
                DirectX::XMLoadFloat4x4(&projection_)));
    }

    void Camera::set_aperture_fit(ApertureFit fit) noexcept {
        aperture_fit_ = fit;
        update_projection();

        DirectX::XMStoreFloat4x4(
            &view_projection_,
            DirectX::XMMatrixMultiply(
                DirectX::XMLoadFloat4x4(&view_),
                DirectX::XMLoadFloat4x4(&projection_)));
    }

    void Camera::frame_bounds(const math::AABB& bounds) noexcept {
        using namespace DirectX;

        XMFLOAT3 center{};
        XMFLOAT3 size{1.0f, 1.0f, 1.0f};
        if (bounds.is_valid()) {
            center = bounds.get_center();
            size = bounds.get_size();
        }

        const float radius = std::max({
            size.x,
            size.y,
            size.z,
            1.0f,
        });
        const XMVECTOR position = XMVectorSet(
            center.x,
            center.y + radius * 0.25f,
            center.z - radius * 1.75f,
            1.0f);
        const XMMATRIX view = XMMatrixLookAtLH(
            position,
            XMLoadFloat3(&center),
            XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
        const XMMATRIX world = XMMatrixInverse(nullptr, view);
        XMStoreFloat4x4(&source_.world_transform, world);

        if (!positive_finite(source_.focal_length)) {
            source_.focal_length = DEFAULT_FOCAL_LENGTH;
        }
        if (!positive_finite(source_.horizontal_aperture)) {
            source_.horizontal_aperture =
                DEFAULT_HORIZONTAL_APERTURE;
        }
        if (!positive_finite(source_.vertical_aperture)) {
            source_.vertical_aperture =
                DEFAULT_VERTICAL_APERTURE;
        }

        source_.clipping_range = {
            std::max(0.01f, radius * 0.001f),
            std::max(100.0f, radius * 10.0f),
        };
        update();
    }

    bool Camera::has_valid_lens() const noexcept {
        return
            positive_finite(source_.focal_length) &&
            positive_finite(source_.horizontal_aperture) &&
            positive_finite(source_.vertical_aperture) &&
            positive_finite(source_.clipping_range.x) &&
            std::isfinite(source_.clipping_range.y) &&
            source_.clipping_range.y > source_.clipping_range.x;
    }

    float Camera::get_aspect_ratio() const noexcept {
        return static_cast<float>(viewport_width_) /
            static_cast<float>(viewport_height_);
    }

    void Camera::update() noexcept {
        update_view();
        update_projection();

        DirectX::XMStoreFloat4x4(
            &view_projection_,
            DirectX::XMMatrixMultiply(
                DirectX::XMLoadFloat4x4(&view_),
                DirectX::XMLoadFloat4x4(&projection_)));
    }

    void Camera::update_view() noexcept {
        using namespace DirectX;

        world_ = source_.world_transform;
        const XMMATRIX world = XMLoadFloat4x4(&world_);
        XMVECTOR determinant;
        const XMMATRIX inverse = XMMatrixInverse(&determinant, world);

        const float determinant_value = XMVectorGetX(determinant);
        valid_transform_ =
            std::isfinite(determinant_value) &&
            std::isfinite(world_._11) &&
            std::isfinite(world_._12) &&
            std::isfinite(world_._13) &&
            std::isfinite(world_._14) &&
            std::isfinite(world_._21) &&
            std::isfinite(world_._22) &&
            std::isfinite(world_._23) &&
            std::isfinite(world_._24) &&
            std::isfinite(world_._31) &&
            std::isfinite(world_._32) &&
            std::isfinite(world_._33) &&
            std::isfinite(world_._34) &&
            std::isfinite(world_._41) &&
            std::isfinite(world_._42) &&
            std::isfinite(world_._43) &&
            std::isfinite(world_._44) &&
            std::abs(determinant_value) > MINIMUM_VALUE;
        if (valid_transform_) {
            XMStoreFloat4x4(&view_, inverse);
        } else {
            view_ = scene::StaticScene::IDENTITY_TRANSFORM;
        }

        world_position_ = {
            world_._41,
            world_._42,
            world_._43,
        };
    }

    void Camera::update_projection() noexcept {
        using namespace DirectX;

        const float focal_length = positive_finite(source_.focal_length)
            ? source_.focal_length
            : DEFAULT_FOCAL_LENGTH;
        float horizontal_aperture =
            positive_finite(source_.horizontal_aperture)
            ? source_.horizontal_aperture
            : DEFAULT_HORIZONTAL_APERTURE;
        float vertical_aperture =
            positive_finite(source_.vertical_aperture)
            ? source_.vertical_aperture
            : DEFAULT_VERTICAL_APERTURE;

        const float viewport_aspect = get_aspect_ratio();
        const float aperture_aspect =
            horizontal_aperture / vertical_aperture;

        ApertureFit resolved_fit = aperture_fit_;
        if (resolved_fit == ApertureFit::OVERSCAN) {
            resolved_fit = viewport_aspect >= aperture_aspect
                ? ApertureFit::VERTICAL
                : ApertureFit::HORIZONTAL;
        } else if (resolved_fit == ApertureFit::FILL) {
            resolved_fit = viewport_aspect >= aperture_aspect
                ? ApertureFit::HORIZONTAL
                : ApertureFit::VERTICAL;
        }

        if (resolved_fit == ApertureFit::HORIZONTAL) {
            vertical_aperture =
                horizontal_aperture / viewport_aspect;
        } else {
            horizontal_aperture =
                vertical_aperture * viewport_aspect;
        }

        near_plane_ = positive_finite(source_.clipping_range.x)
            ? source_.clipping_range.x
            : DEFAULT_NEAR_PLANE;
        far_plane_ =
            std::isfinite(source_.clipping_range.y) &&
            source_.clipping_range.y > near_plane_
            ? source_.clipping_range.y
            : DEFAULT_FAR_PLANE;

        const float horizontal_scale =
            near_plane_ / focal_length;
        const float vertical_scale =
            near_plane_ / focal_length;
        const float left = horizontal_scale *
            (-0.5f * horizontal_aperture +
                source_.horizontal_aperture_offset);
        const float right = horizontal_scale *
            (0.5f * horizontal_aperture +
                source_.horizontal_aperture_offset);
        const float bottom = vertical_scale *
            (-0.5f * vertical_aperture +
                source_.vertical_aperture_offset);
        const float top = vertical_scale *
            (0.5f * vertical_aperture +
                source_.vertical_aperture_offset);

        XMStoreFloat4x4(
            &projection_,
            XMMatrixPerspectiveOffCenterLH(
                left,
                right,
                bottom,
                top,
                near_plane_,
                far_plane_));
    }

} // namespace fjr::render
