#pragma once

#include <cstdint>
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
        DirectX::ScratchImage& compressed);

} // namespace fjr::cooker
