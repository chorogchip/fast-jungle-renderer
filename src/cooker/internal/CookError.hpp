#pragma once

#include <string>
#include <utility>

#include "FastJungle/core/util/Logger.hpp"

namespace fjr::cooker::internal {

    [[noreturn]] inline void fail(std::string message) {
        log::Logger::g_logger << log::abrt(message);
    }

    template<typename... Parts>
    [[noreturn]] void fail(Parts&&... parts) {
        std::string message;
        (message.append(std::forward<Parts>(parts)), ...);
        fail(std::move(message));
    }

} // namespace fjr::cooker::internal
