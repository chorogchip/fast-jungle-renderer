#pragma once

#include <array>
#include <cmath>

#include <DirectXMath.h>

#include "FastJungle/core/math/AABB.hpp"

namespace fjr::math {

    class Frustum final {
    public:
        explicit Frustum(
            const DirectX::XMFLOAT4X4& view_projection) noexcept
            : view_projection_(view_projection) {}

        [[nodiscard]]
        bool intersects(const AABB& bounds) const noexcept {

            if (!bounds.is_valid()) {
                return true;
            }

            for (const auto& plane : planes()) {
                const float x = plane.x >= 0.0f ? bounds.max.x : bounds.min.x;
                const float y = plane.y >= 0.0f ? bounds.max.y : bounds.min.y;
                const float z = plane.z >= 0.0f ? bounds.max.z : bounds.min.z;
                if (plane.x * x + plane.y * y + plane.z * z + plane.w < 0.0f) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]]
        bool intersects(
            const DirectX::XMFLOAT3& center,
            float radius) const noexcept {

            for (const auto& plane : planes()) {
                if (plane.x * center.x +
                    plane.y * center.y +
                    plane.z * center.z +
                    plane.w < -radius) {

                    return false;
                }
            }
            return true;
        }

    private:
        [[nodiscard]]
        std::array<DirectX::XMFLOAT4, 6> planes() const noexcept {

            const auto& m = view_projection_;
            std::array<DirectX::XMFLOAT4, 6> result{
                DirectX::XMFLOAT4{m._11 + m._14, m._21 + m._24, m._31 + m._34, m._41 + m._44},
                DirectX::XMFLOAT4{-m._11 + m._14, -m._21 + m._24, -m._31 + m._34, -m._41 + m._44},
                DirectX::XMFLOAT4{m._12 + m._14, m._22 + m._24, m._32 + m._34, m._42 + m._44},
                DirectX::XMFLOAT4{-m._12 + m._14, -m._22 + m._24, -m._32 + m._34, -m._42 + m._44},
                DirectX::XMFLOAT4{m._13, m._23, m._33, m._43},
                DirectX::XMFLOAT4{-m._13 + m._14, -m._23 + m._24, -m._33 + m._34, -m._43 + m._44},
            };
            for (auto& plane : result) {
                const float length = std::sqrt(
                    plane.x * plane.x +
                    plane.y * plane.y +
                    plane.z * plane.z);
                if (length != 0.0f) {
                    plane.x /= length;
                    plane.y /= length;
                    plane.z /= length;
                    plane.w /= length;
                }
            }
            return result;
        }

        DirectX::XMFLOAT4X4 view_projection_{};
    };

} // namespace fjr::math
