#include "FastJungle/core/util/TemporaryFile.hpp"

#include "FastJungle/core/util/Logger.hpp"

#include <Windows.h>

#include <system_error>
#include <utility>

namespace fjr::util {

    TemporaryFile::TemporaryFile(std::filesystem::path path)
        : path_(std::move(path)) {}

    TemporaryFile::TemporaryFile(TemporaryFile&& other) noexcept
        : path_(std::move(other.path_)),
          active_(std::exchange(other.active_, false)) {}

    TemporaryFile::~TemporaryFile() {
        if (!active_) {
            return;
        }

        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    const std::filesystem::path& TemporaryFile::path() const noexcept {
        return path_;
    }

    void TemporaryFile::remove() {
        std::error_code error;
        std::filesystem::remove(path_, error);
        if (error) {
            log::Logger::g_logger
                << "Failed to remove temporary file: " << path_ << '\n'
                << error.message() << '\n';
            log::Logger::g_logger.abort();
        }
        active_ = false;
    }

    void TemporaryFile::replace(
        const std::filesystem::path& destination) {

        if (!MoveFileExW(
            path_.c_str(),
            destination.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            log::Logger::g_logger
                << "Failed to replace file: " << destination << '\n'
                << "  Windows error: " << GetLastError() << '\n';
            log::Logger::g_logger.abort();
        }
        active_ = false;
    }

} // namespace fjr::util
