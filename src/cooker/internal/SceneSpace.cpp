#include "SceneSpace.hpp"

#include "CookError.hpp"

#include "FastJungle/scene/StaticScene.hpp"

#include <cmath>

namespace fjr::cooker::internal {

    namespace {

        constexpr float VECTOR_EPSILON = 1.0e-10f;

    } // namespace

    SceneSpace::SceneSpace(double meters_per_unit)
        : meters_per_unit_(static_cast<float>(meters_per_unit)) {

        if (!(meters_per_unit_ > 0.0f) ||
            !std::isfinite(meters_per_unit_)) {
            fail("Invalid stage metersPerUnit.");
        }

        const float inverse = 1.0f / meters_per_unit_;
        source_to_target_ = DirectX::XMMatrixSet(
            meters_per_unit_, 0.0f,             0.0f,             0.0f,
            0.0f,             0.0f,             meters_per_unit_, 0.0f,
            0.0f,             meters_per_unit_, 0.0f,             0.0f,
            0.0f,             0.0f,             0.0f,             1.0f);
        target_to_source_ = DirectX::XMMatrixSet(
            inverse, 0.0f,    0.0f,    0.0f,
            0.0f,    0.0f,    inverse, 0.0f,
            0.0f,    inverse, 0.0f,    0.0f,
            0.0f,    0.0f,    0.0f,    1.0f);
    }

    DirectX::XMFLOAT3 SceneSpace::position_to_meters(
        const pxr::GfVec3f& source) const noexcept {

        return {
            source[0] * meters_per_unit_,
            source[2] * meters_per_unit_,
            source[1] * meters_per_unit_};
    }

    DirectX::XMFLOAT3 SceneSpace::direction_to_target(
        const pxr::GfVec3f& source) const noexcept {

        DirectX::XMFLOAT3 result{source[0], source[2], source[1]};
        const auto value = DirectX::XMLoadFloat3(&result);
        const float length_squared = DirectX::XMVectorGetX(
            DirectX::XMVector3LengthSq(value));
        if (length_squared > VECTOR_EPSILON) {
            DirectX::XMStoreFloat3(
                &result,
                DirectX::XMVector3Normalize(value));
        }
        return result;
    }

    DirectX::XMFLOAT4 SceneSpace::orientation_to_target(
        const pxr::GfQuath& source) const noexcept {

        const auto imaginary = source.GetImaginary();
        DirectX::XMFLOAT4 result{
            -static_cast<float>(imaginary[0]),
            -static_cast<float>(imaginary[2]),
            -static_cast<float>(imaginary[1]),
            static_cast<float>(source.GetReal())};

        auto quaternion = DirectX::XMLoadFloat4(&result);
        if (DirectX::XMVectorGetX(
            DirectX::XMVector4LengthSq(quaternion)) <= VECTOR_EPSILON) {
            return {0.0f, 0.0f, 0.0f, 1.0f};
        }
        DirectX::XMStoreFloat4(
            &result,
            DirectX::XMQuaternionNormalize(quaternion));
        return result;
    }

    DirectX::XMFLOAT3 SceneSpace::scale_to_target(
        const pxr::GfVec3f& source) const noexcept {

        return {source[0], source[2], source[1]};
    }

    float SceneSpace::distance_to_meters(float source) const noexcept {
        return source * meters_per_unit_;
    }

    DirectX::XMFLOAT4X4 SceneSpace::camera_pose_to_target(
        const pxr::GfMatrix4d& source_view_inverse) const noexcept {

        const auto converted_direction =
            [this, &source_view_inverse](int row, float sign) {
                return direction_to_target({
                    sign * static_cast<float>(source_view_inverse[row][0]),
                    sign * static_cast<float>(source_view_inverse[row][1]),
                    sign * static_cast<float>(source_view_inverse[row][2])});
            };

        const auto right = converted_direction(0, 1.0f);
        const auto up = converted_direction(1, 1.0f);
        // UsdGeomCamera looks down local -Z; StaticScene looks down +Z.
        const auto forward = converted_direction(2, -1.0f);

        DirectX::XMFLOAT4X4 result =
            scene::StaticScene::IDENTITY_TRANSFORM;
        result._11 = right.x;
        result._12 = right.y;
        result._13 = right.z;
        result._21 = up.x;
        result._22 = up.y;
        result._23 = up.z;
        result._31 = forward.x;
        result._32 = forward.y;
        result._33 = forward.z;
        result._41 = static_cast<float>(source_view_inverse[3][0]) *
            meters_per_unit_;
        result._42 = static_cast<float>(source_view_inverse[3][2]) *
            meters_per_unit_;
        result._43 = static_cast<float>(source_view_inverse[3][1]) *
            meters_per_unit_;
        return result;
    }

    DirectX::XMMATRIX SceneSpace::object_transform_to_target(
        const pxr::GfMatrix4d& source) const noexcept {

        DirectX::XMFLOAT4X4 stored;
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                stored.m[row][column] = static_cast<float>(
                    source[row][column]);
            }
        }

        const auto source_matrix = DirectX::XMLoadFloat4x4(&stored);
        return DirectX::XMMatrixMultiply(
            DirectX::XMMatrixMultiply(
                target_to_source_,
                source_matrix),
            source_to_target_);
    }

    DirectX::XMFLOAT4X4 SceneSpace::store(
        DirectX::FXMMATRIX source) noexcept {

        DirectX::XMFLOAT4X4 result;
        DirectX::XMStoreFloat4x4(&result, source);
        return result;
    }

} // namespace fjr::cooker::internal
