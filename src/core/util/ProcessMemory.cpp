#include "FastJungle/core/util/ProcessMemory.hpp"

#include <Windows.h>
#include <Psapi.h>

namespace fjr::util {

    std::optional<ProcessMemory> ProcessMemory::query() noexcept {
        PROCESS_MEMORY_COUNTERS_EX counters{};
        counters.cb = sizeof(counters);
        if (!GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters))) {
            return std::nullopt;
        }

        return ProcessMemory{
            .private_bytes = counters.PrivateUsage,
            .working_set_bytes = counters.WorkingSetSize,
            .peak_working_set_bytes = counters.PeakWorkingSetSize
        };
    }

} // namespace fjr::util
