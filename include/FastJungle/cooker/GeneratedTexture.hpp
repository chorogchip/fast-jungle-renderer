#pragma once

#include <DirectXTex.h>
#include <dxgiformat.h>

#include <string>

namespace fjr::cooker {

    // An image produced by an offline cooker pass.  It still becomes a normal
    // StaticScene texture and is written into the regular .fjtex payload;
    // this type only avoids a temporary image file between those two steps.
    struct GeneratedTexture final {
        std::string key;
        DirectX::ScratchImage image;
        DXGI_FORMAT uncompressed_output_format = DXGI_FORMAT_UNKNOWN;
    };

} // namespace fjr::cooker
