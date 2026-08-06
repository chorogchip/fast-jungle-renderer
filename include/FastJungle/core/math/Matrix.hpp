#pragma once

#include <algorithm>
#include <cmath>

#include <DirectXMath.h>

namespace fjr::math {

    class Matrix final {
    public:
        [[nodiscard]]
        static float XM_CALLCONV maximum_scale(DirectX::FXMMATRIX matrix) noexcept {
            DirectX::XMFLOAT4X4 data;
            DirectX::XMStoreFloat4x4(&data, matrix);
            const auto length = [](float x, float y, float z) noexcept {
                return std::sqrt(x * x + y * y + z * z);
            };
            return std::max({
                length(data._11, data._12, data._13),
                length(data._21, data._22, data._23),
                length(data._31, data._32, data._33),
            });
        }
    };

} // namespace fjr::math
