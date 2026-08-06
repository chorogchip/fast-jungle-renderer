#pragma once

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>
#include <DirectXMath.h>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

namespace fjr::math {

    struct AABB {

        DirectX::XMFLOAT3 min;
        DirectX::XMFLOAT3 max;

        static constexpr float INF =
            std::numeric_limits<float>::infinity();

        constexpr AABB() noexcept
            : min{ INF, INF, INF },
            max{ -INF, -INF, -INF } {}

        constexpr AABB(
            float min_x, float min_y, float min_z,
            float max_x, float max_y, float max_z) noexcept
            : min{ min_x, min_y, min_z }, max{ max_x, max_y, max_z } {}

        constexpr AABB(
            const DirectX::XMFLOAT3& min_point,
            const DirectX::XMFLOAT3& max_point) noexcept
            : min{ min_point }, max{ max_point } {}

        void reset() noexcept { min = { INF, INF, INF }; max = { -INF, -INF, -INF }; }

        [[nodiscard]]  DirectX::XMVECTOR get_min() const noexcept {
            return DirectX::XMLoadFloat3(&min);
        }
        [[nodiscard]]  DirectX::XMVECTOR get_max() const noexcept {
            return DirectX::XMLoadFloat3(&max);
        }

        [[nodiscard]]
        constexpr bool is_valid() const noexcept {
            return
                min.x <= max.x &&
                min.y <= max.y &&
                min.z <= max.z;
        }

        constexpr void merge(float x, float y, float z) noexcept {
            min.x = std::min(min.x, x);
            min.y = std::min(min.y, y);
            min.z = std::min(min.z, z);

            max.x = std::max(max.x, x);
            max.y = std::max(max.y, y);
            max.z = std::max(max.z, z);
        }

        constexpr void merge(const DirectX::XMFLOAT3& point) noexcept {
            this->merge(point.x, point.y, point.z);
        }

        constexpr void merge(const AABB& other) noexcept {
            if (!other.is_valid()) return;
            this->merge(other.min);
            this->merge(other.max);
        }

        [[nodiscard]]
        constexpr DirectX::XMFLOAT3 get_center() const noexcept {
            return {
                (min.x + max.x) * 0.5f,
                (min.y + max.y) * 0.5f,
                (min.z + max.z) * 0.5f
            };
        }

        [[nodiscard]]
        constexpr DirectX::XMFLOAT3 get_size() const noexcept {
            return {
                max.x - min.x,
                max.y - min.y,
                max.z - min.z
            };
        }

        [[nodiscard]]
        float distance_to(const DirectX::XMFLOAT3& point) const noexcept {
            if (!is_valid()) {
                return 0.0f;
            }

            const float x = std::max({ min.x - point.x, 0.0f, point.x - max.x });
            const float y = std::max({ min.y - point.y, 0.0f, point.y - max.y });
            const float z = std::max({ min.z - point.z, 0.0f, point.z - max.z });
            return std::sqrt(x * x + y * y + z * z);
        }

        [[nodiscard]]
        constexpr bool do_contain(float x, float y, float z) const noexcept {
            return
                x >= min.x && x <= max.x &&
                y >= min.y && y <= max.y &&
                z >= min.z && z <= max.z;
        }

        [[nodiscard]]
        constexpr bool do_contain(const DirectX::XMFLOAT3& point) const noexcept {
            return this->do_contain(point.x, point.y, point.z);
        }

        [[nodiscard]]
        constexpr bool do_contain(const AABB& other) const noexcept {
            return
                other.min.x >= min.x && other.max.x <= max.x &&
                other.min.y >= min.y && other.max.y <= max.y &&
                other.min.z >= min.z && other.max.z <= max.z;
        }

        [[nodiscard]]
        constexpr bool do_intersect(const AABB& other) const noexcept {
            return
                min.x <= other.max.x && max.x >= other.min.x &&
                min.y <= other.max.y && max.y >= other.min.y &&
                min.z <= other.max.z && max.z >= other.min.z;
        }

        void XM_CALLCONV transform(DirectX::FXMMATRIX matrix) noexcept {
            using namespace DirectX;
            if (!is_valid()) return;

            const XMVECTOR min_v = XMLoadFloat3(&min);
            const XMVECTOR max_v = XMLoadFloat3(&max);

            const XMVECTOR center = XMVectorScale(XMVectorAdd(min_v, max_v), 0.5f);
            const XMVECTOR extents = XMVectorScale(XMVectorSubtract(max_v, min_v), 0.5f);

            const XMVECTOR center_new = XMVector3TransformCoord(center, matrix);

            const XMVECTOR mask = XMVectorSetInt(
                0x7FFFFFFF,
                0x7FFFFFFF,
                0x7FFFFFFF,
                0x7FFFFFFF
            );

            const XMVECTOR axis_x = XMVectorAndInt(matrix.r[0], mask);
            const XMVECTOR axis_y = XMVectorAndInt(matrix.r[1], mask);
            const XMVECTOR axis_z = XMVectorAndInt(matrix.r[2], mask);

            XMVECTOR extents_new = XMVectorMultiply(axis_x, XMVectorSplatX(extents));
            extents_new = XMVectorMultiplyAdd(axis_y, XMVectorSplatY(extents), extents_new);
            extents_new = XMVectorMultiplyAdd(axis_z, XMVectorSplatZ(extents), extents_new);

            XMStoreFloat3(&min, XMVectorSubtract(center_new, extents_new));
            XMStoreFloat3(&max, XMVectorAdd(center_new, extents_new));
        }

        [[nodiscard]]
        AABB XM_CALLCONV transformed(DirectX::FXMMATRIX matrix) const noexcept {
            auto result = *this;
            result.transform(matrix);
            return result;
        }

        [[nodiscard]]
        static AABB build_from_vertices(
            const float* vertices,
            std::size_t vertex_count,
            std::size_t stride_floats = 3) noexcept {

            AABB result{};

            if (vertices == nullptr || vertex_count == 0 || stride_floats < 3) {
                return result;
            }

            for (std::size_t i = 0; i < vertex_count; ++i) {
                const float* vertex = vertices + i * stride_floats;
                result.merge(vertex[0], vertex[1], vertex[2]);
            }

            return result;
        }
    };
}
