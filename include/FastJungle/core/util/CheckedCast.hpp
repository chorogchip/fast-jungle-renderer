#pragma once

#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "FastJungle/core/util/Logger.hpp"

namespace fjr::util {

    template<std::integral To, std::integral From>
        requires (
            !std::same_as<std::remove_cv_t<To>, bool> &&
            !std::same_as<std::remove_cv_t<From>, bool>)
    [[nodiscard]] To checked_cast(
        From value,
        std::string_view subject) {

        if (!std::in_range<To>(value)) {
            log::Logger::g_logger << log::abrt(
                std::string{subject} +
                " does not fit the destination integer type.");
        }
        return static_cast<To>(value);
    }

    template<std::integral From>
        requires (!std::same_as<std::remove_cv_t<From>, bool>)
    [[nodiscard]] uint32_t checked_u32(
        From value,
        std::string_view subject) {
        return checked_cast<uint32_t>(value, subject);
    }

} // namespace fjr::util
