#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace fjr::cooker {
    template<typename... Parts>
    [[noreturn]] void fail(Parts&&... parts) {
        std::string message;
        (message.append(std::forward<Parts>(parts)), ...);
        throw std::runtime_error(std::move(message));
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
