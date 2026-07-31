#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/core/util/DateTime.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

namespace fjr::log {

    Logger Logger::g_logger;

    void Logger::flush_unlocked() {
        const std::string log = logging_stream_.str();

        if (log.empty()) {
            return;
        }

        try {
            const std::filesystem::path log_directory =
                std::filesystem::current_path() / "logs";

            std::error_code error;

            std::filesystem::create_directories(
                log_directory,
                error);

            if (error) {
                std::cerr
                    << "[LOGGER ERROR] failed to create log directory: "
                    << error.message()
                    << '\n';

                std::cerr << log;
                std::cerr.flush();
                return;
            }

            const std::string filename =
                util::DateTime::now_string() + ".log";

            const std::filesystem::path log_path =
                log_directory / filename;

            std::ofstream output{
                log_path,
                std::ios::out |
                std::ios::binary |
                std::ios::trunc
            };

            if (!output.is_open()) {
                std::cerr
                    << "[LOGGER ERROR] failed to open log file: "
                    << log_path
                    << '\n';

                std::cerr << log;
                std::cerr.flush();
                return;
            }

            output.write(
                log.data(),
                static_cast<std::streamsize>(log.size()));

            output.flush();

            if (!output) {
                std::cerr
                    << "[LOGGER ERROR] failed to write log file: "
                    << log_path
                    << '\n';

                std::cerr << log;
                std::cerr.flush();
                return;
            }

            logging_stream_.str({});
            logging_stream_.clear();

            std::cerr
                << "[LOGGER] log saved: "
                << log_path
                << '\n';
        } catch (const std::exception& exception) {
            std::cerr
                << "[LOGGER ERROR] "
                << exception.what()
                << '\n';

            std::cerr << log;
            std::cerr.flush();
        }
    }

    void Logger::flush() {
        std::lock_guard lock(mutex_);
        flush_unlocked();
    }

    [[noreturn]]
    void Logger::abort_unlocked(std::source_location loc) {
        logging_stream_
            << '\n'
            << "[FATAL]\n"
            << "  file:     " << loc.file_name() << '\n'
            << "  line:     " << loc.line() << '\n'
            << "  column:   " << loc.column() << '\n'
            << "  function: " << loc.function_name() << '\n';

        flush_unlocked();

        std::abort();
    }

    [[noreturn]]
    void Logger::abort(std::source_location loc) {
        std::lock_guard lock(mutex_);
        abort_unlocked(loc);
    }

    Logger& Logger::operator<<(
        std::ostream& (*manip)(std::ostream&)) {
        std::lock_guard lock(mutex_);
        manip(logging_stream_);
        return *this;
    }

    Logger& Logger::operator<<(const DoAssert& assertion) {
        if (!assertion.to_abort) {
            return *this;
        }

        std::lock_guard lock(mutex_);

        if (!assertion.message.empty()) {
            logging_stream_
                << "[ASSERT] "
                << assertion.message
                << '\n';
        } else {
            logging_stream_
                << "[ASSERT] assertion failed\n";
        }

        abort_unlocked(assertion.loc);
    }

} // namespace fjr::log