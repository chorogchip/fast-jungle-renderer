#pragma once

#include <filesystem>

namespace fjr::util {

    class TemporaryFile final {
    public:
        explicit TemporaryFile(std::filesystem::path path);
        ~TemporaryFile();

        TemporaryFile(const TemporaryFile&) = delete;
        TemporaryFile& operator=(const TemporaryFile&) = delete;
        TemporaryFile(TemporaryFile&& other) noexcept;
        TemporaryFile& operator=(TemporaryFile&&) = delete;

        [[nodiscard]]
        const std::filesystem::path& path() const noexcept;

        void remove();
        void replace(const std::filesystem::path& destination);

    private:
        std::filesystem::path path_;
        bool active_ = true;
    };

} // namespace fjr::util
