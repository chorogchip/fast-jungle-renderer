#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace fjr::cooker::internal {

    [[noreturn]] inline void fail(std::string message) {
        throw std::runtime_error{std::move(message)};
    }

    template<typename... Parts>
    [[noreturn]] void fail(Parts&&... parts) {
        std::string message;
        (message.append(std::forward<Parts>(parts)), ...);
        fail(std::move(message));
    }

} // namespace fjr::cooker::internal
