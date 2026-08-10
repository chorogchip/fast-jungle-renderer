#pragma once

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>

#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/data/DataPersistent.hpp"

namespace fjr::render::data::geom {

    namespace {

        uint32_t normal_to_10u(float value) {
            const float normalized = std::clamp(
                value * 0.5f + 0.5f, 0.0f, 1.0f);
            return static_cast<uint32_t>(
                std::lround(normalized * 1023.0f));
        }
    }

    class Packing {

    public:
        static DataPersistent::PackedNormal pack_normal(const DirectX::XMFLOAT3& vec) {
            DataPersistent::PackedNormal ret{};
            ret.value =
                normal_to_10u(vec.x) |
                (normal_to_10u(vec.y) << 10u) |
                (normal_to_10u(vec.z) << 20u) |
                (3u << 30u);
            return ret;
        }
    };

    [[nodiscard]]
    std::uint16_t quantize_unorm16(
        float value,
        float minimum,
        float extent) {

        const double normalized =
            (static_cast<double>(value) -
                static_cast<double>(minimum)) /
            static_cast<double>(extent);

        const double clamped = std::clamp(normalized, 0.0, 1.0);
        const auto q = static_cast<std::uint32_t>(std::floor(clamped * 65535.0 + 0.5));
        return static_cast<std::uint16_t>(std::min(q, 65535u));
    }
}