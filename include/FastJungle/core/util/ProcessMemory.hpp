#pragma once

#include <cstdint>
#include <optional>

namespace fjr::util {

    class ProcessMemory final {
    public:
        std::uint64_t private_bytes = 0;
        std::uint64_t working_set_bytes = 0;
        std::uint64_t peak_working_set_bytes = 0;

        [[nodiscard]]
        static std::optional<ProcessMemory> query() noexcept;
    };

} // namespace fjr::util
