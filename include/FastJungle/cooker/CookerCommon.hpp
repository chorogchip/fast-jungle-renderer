#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "FastJungle/core/util/Logger.hpp"

namespace fjr::cooker {
    template<typename... Parts>
    [[noreturn]] void fail(Parts&&... parts) {
        std::string message;
        (message.append(std::forward<Parts>(parts)), ...);
        log::Logger::g_logger << log::abrt(message);
    }

    [[nodiscard]] inline std::uint32_t checked_u32(
        std::size_t value,
        std::string_view subject) {
        if (value > std::numeric_limits<std::uint32_t>::max()) {
            fail(std::string{subject}, " exceeds uint32_t.");
        }
        return static_cast<std::uint32_t>(value);
    }

} // namespace fjr::cooker
