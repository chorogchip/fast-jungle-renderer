#pragma once

#include <array>
#include <cstdint>
#include <filesystem>

namespace fjr::util {

    struct Sha256 final {
        std::array<uint8_t, 32> bytes{};

        [[nodiscard]] bool operator==(const Sha256&) const noexcept = default;
    };

    class FileHash final {
    public:
        FileHash() = delete;

        [[nodiscard]]
        static Sha256 sha256(const std::filesystem::path& path);
    };

} // namespace fjr::util
