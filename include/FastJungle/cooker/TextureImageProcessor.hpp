#pragma once

#include <cstdint>

#include <DirectXTex.h>

#include "FastJungle/cooker/TextureCompressionPolicy.hpp"

namespace fjr::cooker {

    class TextureImageProcessor final {
    public:
        TextureImageProcessor() = delete;

        [[nodiscard]]
        static std::uint64_t estimate_working_memory(
            const DirectX::TexMetadata& metadata,
            const TextureCompressionPlan& plan);

        static void process(
            DirectX::ScratchImage& decoded,
            const TextureCompressionPlan& plan,
            bool fast_bc7,
            DirectX::ScratchImage& compressed);
    };

} // namespace fjr::cooker
