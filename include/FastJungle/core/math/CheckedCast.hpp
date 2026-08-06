#pragma once

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "FastJungle/core/util/Logger.hpp"

namespace fjr::math {

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

} // namespace fjr::math
