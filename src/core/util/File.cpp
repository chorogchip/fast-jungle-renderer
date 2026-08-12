#include "FastJungle/core/util/File.hpp"

#include "FastJungle/core/util/Logger.hpp"

#include <system_error>

namespace fjr::util {

    uint64_t File::size(const std::filesystem::path& path) {
        std::error_code error;
        const auto result = std::filesystem::file_size(path, error);
        if (error) {
            log::Logger::g_logger
                << "Failed to measure file: " << path << '\n'
                << error.message() << '\n';
            log::Logger::g_logger.abort();
        }
        return result;
    }

    std::ifstream File::open_read(const std::filesystem::path& path) {
        std::ifstream result{path, std::ios::binary};
        if (!result.is_open()) {
            log::Logger::g_logger
                << "Failed to open file for reading: " << path << '\n';
            log::Logger::g_logger.abort();
        }
        return result;
    }

    std::ofstream File::open_write(const std::filesystem::path& path) {
        std::ofstream result{
            path,
            std::ios::binary | std::ios::trunc
        };
        if (!result.is_open()) {
            log::Logger::g_logger
                << "Failed to open file for writing: " << path << '\n';
            log::Logger::g_logger.abort();
        }
        return result;
    }

    void File::finish(
        std::ofstream& output,
        const std::filesystem::path& path) {

        output.flush();
        if (!output) {
            log::Logger::g_logger
                << "Failed to flush file: " << path << '\n';
            log::Logger::g_logger.abort();
        }

        output.close();
        if (!output) {
            log::Logger::g_logger
                << "Failed to close file: " << path << '\n';
            log::Logger::g_logger.abort();
        }
    }

    void File::require_size(
        const std::filesystem::path& path,
        uint64_t expected) {

        const uint64_t actual = size(path);
        if (actual != expected) {
            log::Logger::g_logger
                << "File size is invalid: " << path << '\n'
                << "  expected: " << expected << '\n'
                << "  actual: " << actual << '\n';
            log::Logger::g_logger.abort();
        }
    }

    void File::create_directories(const std::filesystem::path& path) {
        std::error_code error;
        std::filesystem::create_directories(path, error);
        if (error) {
            log::Logger::g_logger
                << "Failed to create directory: " << path << '\n'
                << error.message() << '\n'
                << log::abrt();
        }
    }

} // namespace fjr::util
