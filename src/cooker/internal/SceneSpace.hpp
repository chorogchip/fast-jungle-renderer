#pragma once

#include <DirectXMath.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/quath.h>
#include <pxr/base/gf/vec3f.h>

namespace fjr::cooker::internal {

    class SceneSpace final {
    public:
        explicit SceneSpace(double meters_per_unit);

        [[nodiscard]] DirectX::XMFLOAT3 position_to_meters(
            const pxr::GfVec3f& source) const noexcept;

        [[nodiscard]] DirectX::XMFLOAT3 direction_to_target(
            const pxr::GfVec3f& source) const noexcept;

        [[nodiscard]] DirectX::XMFLOAT4 orientation_to_target(
            const pxr::GfQuath& source) const noexcept;

        [[nodiscard]] DirectX::XMFLOAT3 scale_to_target(
            const pxr::GfVec3f& source) const noexcept;

        [[nodiscard]] float distance_to_meters(float source) const noexcept;

        [[nodiscard]] DirectX::XMFLOAT4X4 camera_pose_to_target(
            const pxr::GfMatrix4d& source_view_inverse) const noexcept;

        [[nodiscard]] DirectX::XMMATRIX object_transform_to_target(
            const pxr::GfMatrix4d& source) const noexcept;

        [[nodiscard]] static DirectX::XMFLOAT4X4 store(
            DirectX::FXMMATRIX source) noexcept;

    private:
        float meters_per_unit_ = 1.0f;
        DirectX::XMMATRIX source_to_target_{};
        DirectX::XMMATRIX target_to_source_{};
    };

} // namespace fjr::cooker::internal
