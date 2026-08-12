#include "FastJungle/core/util/ComInitializer.hpp"

#include "FastJungle/core/util/Logger.hpp"

#include <Windows.h>
#include <objbase.h>

namespace fjr::util {

    ComInitializer::ComInitializer() {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (SUCCEEDED(result)) {
            initialized_ = true;
        }
        else if (result != RPC_E_CHANGED_MODE) {
            log::Logger::g_logger << log::abrt("COM initialization failed.");
        }
    }

    ComInitializer::~ComInitializer() {
        if (initialized_) {
            CoUninitialize();
        }
    }

} // namespace fjr::util
