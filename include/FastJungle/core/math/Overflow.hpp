#pragma once

#include <concepts>
#include <limits>
#include <type_traits>

namespace fjr::math {

    class Overflow final {
    public:
        Overflow() = delete;

        template<std::integral T>
            requires (!std::same_as<std::remove_cv_t<T>, bool>)
        [[nodiscard]]
        static constexpr bool can_add(T a, T b) noexcept {
            constexpr T min = std::numeric_limits<T>::lowest();
            constexpr T max = std::numeric_limits<T>::max();

            if constexpr (std::is_unsigned_v<T>) {
                return b <= max - a;
            } else {
                if (b > 0) {
                    return a <= max - b;
                }

                if (b < 0) {
                    return a >= min - b;
                }

                return true;
            }
        }

        template<std::integral T>
            requires (!std::same_as<std::remove_cv_t<T>, bool>)
        [[nodiscard]]
        static constexpr bool can_sub(T a, T b) noexcept {
            constexpr T min = std::numeric_limits<T>::lowest();
            constexpr T max = std::numeric_limits<T>::max();

            if constexpr (std::is_unsigned_v<T>) {
                return a >= b;
            } else {
                if (b > 0) {
                    return a >= min + b;
                }

                if (b < 0) {
                    return a <= max + b;
                }

                return true;
            }
        }

        template<std::integral T>
            requires (!std::same_as<std::remove_cv_t<T>, bool>)
        [[nodiscard]]
        static constexpr bool can_mul(T a, T b) noexcept {
            constexpr T min = std::numeric_limits<T>::lowest();
            constexpr T max = std::numeric_limits<T>::max();

            if (a == 0 || b == 0) {
                return true;
            }

            if constexpr (std::is_unsigned_v<T>) {
                return a <= max / b;
            } else {
                if ((a == min && b == static_cast<T>(-1)) ||
                    (b == min && a == static_cast<T>(-1))) {
                    return false;
                }

                if (a > 0) {
                    if (b > 0) {
                        return a <= max / b;
                    }

                    return b >= min / a;
                }

                if (b > 0) {
                    return a >= min / b;
                }

                return a >= max / b;
            }
        }
    };

} // namespace fjr::math