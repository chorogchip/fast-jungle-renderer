#pragma once

#include <Windows.h>
#include <source_location>

#include "FastJungle/core/util/Logger.hpp"

namespace fjr::dx {

	inline void abort_failed(
		HRESULT result,
		std::source_location loc = std::source_location::current()) {
		
		if (FAILED(result)) {
			log::Logger::g_logger.abort(loc);
		}
	}
}