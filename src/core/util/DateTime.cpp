#include "FastJungle/core/util/DateTime.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace fjr::util {

    std::string DateTime::now_string(std::string_view format) {
        return to_string(
            std::chrono::system_clock::now(),
            format);
    }

    std::string DateTime::to_string(
        std::chrono::system_clock::time_point time,
        std::string_view format) {
        const std::time_t raw_time =
            std::chrono::system_clock::to_time_t(time);

        std::tm local_time{};

#ifdef _WIN32
        if (::localtime_s(&local_time, &raw_time) != 0) {
            throw std::runtime_error{
                "failed to convert time to local time"
            };
        }
#else
        if (::localtime_r(&raw_time, &local_time) == nullptr) {
            throw std::runtime_error{
                "failed to convert time to local time"
            };
        }
#endif

        std::ostringstream stream;

        stream << std::put_time(
            &local_time,
            std::string{ format }.c_str());

        if (stream.fail()) {
            throw std::runtime_error{
                "failed to format local time"
            };
        }

        return stream.str();
    }

} // namespace fjr::util