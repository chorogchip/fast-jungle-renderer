#pragma once

#include <Windows.h>
#include <cstdint>
#include <iomanip>
#include <source_location>
#include <sstream>

#include "FastJungle/core/util/Logger.hpp"

namespace fjr::dx {

	inline void abort_failed(
		HRESULT result,
		std::source_location loc = std::source_location::current()) {
		
		if (FAILED(result)) {
			std::ostringstream message;
			message << "DirectX call failed with HRESULT 0x"
				<< std::uppercase << std::hex << std::setw(8)
				<< std::setfill('0')
				<< static_cast<std::uint32_t>(result) << ".";
			// Record the HRESULT and abort atomically so another logger flush
			// cannot separate the diagnostic from its fatal location.
			log::Logger::g_logger << log::abrt(message.str(), loc);
		}
	}
}
