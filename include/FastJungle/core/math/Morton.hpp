#pragma once

#include <cstdint>

namespace fjr::math {

    class Morton final {
    public:
        [[nodiscard]]
        static std::uint32_t encode_2d(
            std::uint32_t x,
            std::uint32_t y) noexcept {

            return spread_bits(x) | (spread_bits(y) << 1u);
        }

    private:
        [[nodiscard]]
        static std::uint32_t spread_bits(std::uint32_t value) noexcept {

            value &= 0x0000ffffu;
            value = (value | (value << 8u)) & 0x00ff00ffu;
            value = (value | (value << 4u)) & 0x0f0f0f0fu;
            value = (value | (value << 2u)) & 0x33333333u;
            value = (value | (value << 1u)) & 0x55555555u;
            return value;
        }
    };

} // namespace fjr::math
