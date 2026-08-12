#include "FastJungle/core/util/Process.hpp"

#include "FastJungle/core/util/Logger.hpp"

#include <Windows.h>

#include <string_view>
#include <vector>

namespace fjr::util {

    std::filesystem::path Process::executable_directory() {
        std::vector<wchar_t> buffer(1024);
        for (;;) {
            const DWORD length = GetModuleFileNameW(
                nullptr,
                buffer.data(),
                static_cast<DWORD>(buffer.size()));
            if (length == 0) {
                log::Logger::g_logger << log::abrt(
                    "Failed to get the executable path.");
            }
            if (length < buffer.size() - 1) {
                return std::filesystem::path{
                    std::wstring_view{buffer.data(), length}}
                    .parent_path();
            }
            buffer.resize(buffer.size() * 2);
        }
    }

} // namespace fjr::util
