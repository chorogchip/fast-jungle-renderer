#include "PathKey.hpp"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <system_error>

namespace fjr::cooker::internal {

    std::string path_leaf(std::string_view path) {
        const auto separator = path.find_last_of('/');
        return separator == std::string_view::npos
            ? std::string{path}
            : std::string{path.substr(separator + 1)};
    }

    std::string normalized_path_key(
        const std::filesystem::path& path) {

        std::error_code error;
        auto absolute = std::filesystem::absolute(path, error);
        if (error) {
            absolute = path;
        }

        auto result = absolute.lexically_normal().generic_string();
        std::ranges::transform(
            result,
            result.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
        return result;
    }

} // namespace fjr::cooker::internal
