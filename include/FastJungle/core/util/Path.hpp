#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace fjr::util {

    class Path {
    public:
        using native_type = std::filesystem::path;

        Path() = default;

        explicit Path(const native_type& path)
            : path_{ path } {}

        explicit Path(native_type&& path) noexcept
            : path_{ std::move(path) } {}

        explicit Path(const char* path)
            : path_{ path } {}

        explicit Path(const std::string& path)
            : path_{ path } {}

        Path(const Path&) = default;
        Path(Path&&) noexcept = default;

        Path& operator=(const Path&) = default;
        Path& operator=(Path&&) noexcept = default;

        [[nodiscard]]
        const native_type& native() const noexcept {
            return path_;
        }

        [[nodiscard]]
        bool empty() const noexcept {
            return path_.empty();
        }

        [[nodiscard]]
        Path absolute() const;

        [[nodiscard]]
        Path normalized() const;

        [[nodiscard]]
        Path parent() const;

        [[nodiscard]]
        Path filename() const;

        [[nodiscard]]
        Path extension() const;

        [[nodiscard]]
        Path stem() const;

        [[nodiscard]]
        bool exists() const;

        [[nodiscard]]
        bool is_regular_file() const;

        [[nodiscard]]
        bool is_directory() const;

        [[nodiscard]]
        bool starts_with(std::string_view prefix) const;

        [[nodiscard]]
        bool contains_case_insensitive(std::string_view text) const;

        [[nodiscard]]
        bool filename_contains_case_insensitive(std::string_view text) const;

        [[nodiscard]]
        bool extension_contains_case_insensitive(std::string_view text) const;

        [[nodiscard]]
        bool has_extension_case_insensitive(std::string_view extension) const;

        [[nodiscard]]
        std::string normalized_key() const;

        Path& append(const Path& other);

        [[nodiscard]]
        Path operator/(const Path& other) const;

        Path& operator/=(const Path& other);

        [[nodiscard]]
        std::string string() const {
            return path_.string();
        }

        [[nodiscard]]
        std::string generic_string() const {
            return path_.generic_string();
        }

        explicit operator bool() const noexcept {
            return !path_.empty();
        }

        friend bool operator==(const Path&, const Path&) = default;

    private:
        [[nodiscard]]
        static std::string to_lower(std::string_view value);

    private:
        native_type path_;
    };

    [[nodiscard]]
    std::string path_leaf(std::string_view path);

} // namespace fjr::util
