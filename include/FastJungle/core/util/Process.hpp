#pragma once

#include <filesystem>

namespace fjr::util {

    class Process final {
    public:
        Process() = delete;

        [[nodiscard]]
        static std::filesystem::path executable_directory();
    };

} // namespace fjr::util
