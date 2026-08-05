#pragma once

#include <mutex>
#include <ostream>
#include <source_location>
#include <sstream>
#include <string_view>

namespace fjr::log {

    class Logger {

    public:
        struct DoAssert {
            bool to_abort;
            std::string_view message;
            std::source_location loc;
        };

        Logger() = default;
        ~Logger() = default;

        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        Logger(Logger&&) = delete;
        Logger& operator=(Logger&&) = delete;

        void flush();
        void flush_debug_string();

        [[noreturn]]
        void abort(std::source_location loc = std::source_location::current());

        template<typename T>
        Logger& operator<<(const T& data) {
            std::lock_guard lock(mutex_);
            logging_stream_ << data;
            return *this;
        }

        Logger& operator<<(std::ostream& (*manip)(std::ostream&));

        Logger& operator<<(const DoAssert& assertion);

        static Logger g_logger;
        static Logger g_logger_debug_out;

    private:
        void flush_unlocked();
        void flush_debug_string_unlocked();

        [[noreturn]]
        void abort_unlocked(std::source_location loc = std::source_location::current());

    private:
        std::ostringstream logging_stream_;
        std::mutex mutex_;
    };

    [[nodiscard]]
    inline Logger::DoAssert abrt(
        std::string_view message = {},
        std::source_location loc = std::source_location::current()) {

        return {
            .to_abort = true,
            .message = message,
            .loc = loc
        };
    }

    [[nodiscard]]
    inline Logger::DoAssert asrt(
        bool expression,
        std::string_view message = {},
        std::source_location loc = std::source_location::current()) {

        return {
            .to_abort = !expression,
            .message = message,
            .loc = loc
        };
    }

} // namespace fjr::log