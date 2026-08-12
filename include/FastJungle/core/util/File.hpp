#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>

namespace fjr::util {

    class File final {
    public:
        File() = delete;

        [[nodiscard]]
        static uint64_t size(const std::filesystem::path& path);

        [[nodiscard]]
        static std::ifstream open_read(const std::filesystem::path& path);

        [[nodiscard]]
        static std::ofstream open_write(const std::filesystem::path& path);

        static void finish(
            std::ofstream& output,
            const std::filesystem::path& path);

        static void require_size(
            const std::filesystem::path& path,
            uint64_t expected);

        static void create_directories(const std::filesystem::path& path);
    };

} // namespace fjr::util
