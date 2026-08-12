#pragma once

#include <DirectXTex.h>
#include <dxgiformat.h>

#include <string>

namespace fjr::cooker {

    struct GeneratedTexture final {
        std::string key;
        DirectX::ScratchImage image;
        DXGI_FORMAT uncompressed_output_format = DXGI_FORMAT_UNKNOWN;
    };

} // namespace fjr::cooker
