#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace fjr::cooker::internal {

    [[nodiscard]] std::string path_leaf(std::string_view path);

    [[nodiscard]] std::string normalized_path_key(
        const std::filesystem::path& path);

} // namespace fjr::cooker::internal
