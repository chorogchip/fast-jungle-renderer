#pragma once

#include <concepts>
#include <type_traits>

namespace fjr::enm {

    template<typename T>
        requires std::is_enum_v<T>
    [[nodiscard]] constexpr bool has(T value, T flag) noexcept {
        using U = std::underlying_type_t<T>;
        return (static_cast<U>(value) & static_cast<U>(flag)) != 0;
    }

    template<std::integral T>
    [[nodiscard]] constexpr bool has(T value, T flag) noexcept {
        return (value & flag) != 0;
    }

} // namespace fjr::enm
