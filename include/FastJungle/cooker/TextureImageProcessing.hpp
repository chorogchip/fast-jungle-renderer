#pragma once

#include <cstdint>
#include <string_view>
#include <DirectXTex.h>

#include "FastJungle/cooker/TextureCompression.hpp"

namespace fjr::cooker {

    [[nodiscard]] std::uint64_t estimate_texture_working_memory(
        const DirectX::TexMetadata& metadata,
        const TextureCompressionPlan& plan);

    void process_texture_image(
        DirectX::ScratchImage& decoded,
        const TextureCompressionPlan& plan,
        bool fast_bc7,
        std::string_view source_key,
        DirectX::ScratchImage& compressed);

} // namespace fjr::cooker
