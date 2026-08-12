#include "FastJungle/core/util/Path.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>
#include <utility>

namespace fjr::util {

    Path Path::absolute() const {
        std::error_code error;
        native_type result = std::filesystem::absolute(path_, error);

        if (error) {
            return {};
        }

        return Path{ std::move(result) };
    }

    Path Path::normalized() const {
        return Path{ path_.lexically_normal() };
    }

    Path Path::parent() const {
        return Path{ path_.parent_path() };
    }

    Path Path::filename() const {
        return Path{ path_.filename() };
    }

    Path Path::extension() const {
        return Path{ path_.extension() };
    }

    Path Path::stem() const {
        return Path{ path_.stem() };
    }

    bool Path::exists() const {
        std::error_code error;
        const bool result = std::filesystem::exists(path_, error);

        return !error && result;
    }

    bool Path::is_regular_file() const {
        std::error_code error;
        const bool result = std::filesystem::is_regular_file(path_, error);

        return !error && result;
    }

    bool Path::is_directory() const {
        std::error_code error;
        const bool result = std::filesystem::is_directory(path_, error);

        return !error && result;
    }

    bool Path::starts_with(std::string_view prefix) const {
        return path_.generic_string().starts_with(prefix);
    }

    bool Path::contains_case_insensitive(std::string_view text) const {
        if (text.empty()) {
            return true;
        }

        const std::string path_string = to_lower(path_.generic_string());
        const std::string target = to_lower(text);

        return path_string.find(target) != std::string::npos;
    }

    bool Path::filename_contains_case_insensitive(std::string_view text) const {
        if (text.empty()) {
            return true;
        }

        const std::string filename_string =
            to_lower(path_.filename().generic_string());

        const std::string target = to_lower(text);

        return filename_string.find(target) != std::string::npos;
    }

    bool Path::extension_contains_case_insensitive(std::string_view text) const {
        if (text.empty()) {
            return true;
        }

        const std::string extension_string =
            to_lower(path_.extension().generic_string());

        const std::string target = to_lower(text);

        return extension_string.find(target) != std::string::npos;
    }

    bool Path::has_extension_case_insensitive(
        std::string_view extension) const {
        std::string expected = to_lower(extension);

        if (!expected.empty() && expected.front() != '.') {
            expected.insert(expected.begin(), '.');
        }

        const std::string actual =
            to_lower(path_.extension().generic_string());

        return actual == expected;
    }

    std::string Path::normalized_key() const {
        std::error_code error;
        auto absolute_path = std::filesystem::absolute(path_, error);
        if (error) {
            absolute_path = path_;
        }
        return to_lower(absolute_path.lexically_normal().generic_string());
    }

    Path& Path::append(const Path& other) {
        path_ /= other.path_;
        return *this;
    }

    Path Path::operator/(const Path& other) const {
        return Path{ path_ / other.path_ };
    }

    Path& Path::operator/=(const Path& other) {
        path_ /= other.path_;
        return *this;
    }

    std::string Path::to_lower(std::string_view value) {
        std::string result;
        result.reserve(value.size());

        std::transform(
            value.begin(),
            value.end(),
            std::back_inserter(result),
            [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });

        return result;
    }

    std::string path_leaf(std::string_view path) {
        const auto separator = path.find_last_of("/\\");
        return separator == std::string_view::npos
            ? std::string{path}
            : std::string{path.substr(separator + 1)};
    }

} // namespace fjr::util
