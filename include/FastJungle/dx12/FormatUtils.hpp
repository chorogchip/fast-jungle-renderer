#pragma once

#include <d3d12.h>

namespace fjr::dx {

	class FormatUtils {

	public:
        static DXGI_FORMAT to_bc(DXGI_FORMAT format) noexcept;
		static DXGI_FORMAT to_linear(DXGI_FORMAT format) noexcept;
        static DXGI_FORMAT to_srgb(DXGI_FORMAT format) noexcept;
	};
}