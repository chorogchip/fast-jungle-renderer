#pragma once

#include <chrono>
#include <string>
#include <string_view>

namespace fjr::util {

    class DateTime final {
    public:
        DateTime() = delete;

        [[nodiscard]]
        static std::string now_string(
            std::string_view format = "%Y-%m-%d_%H-%M-%S");

        [[nodiscard]]
        static std::string to_string(
            std::chrono::system_clock::time_point time,
            std::string_view format = "%Y-%m-%d_%H-%M-%S");
    };

} // namespace fjr::util