#pragma once

#include <DirectXTex.h>

#include <string_view>

#include "TextureCompression.hpp"

namespace fjr::cooker {

    class TextureImageBuilder final {
    public:
        TextureImageBuilder() = delete;

        static void build(
            DirectX::ScratchImage& decoded,
            const TextureCompressionPlan& plan,
            std::string_view source_key,
            DirectX::ScratchImage& compressed);
    };

} // namespace fjr::cooker
