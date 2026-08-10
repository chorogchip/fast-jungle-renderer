#pragma once

#include <cstdint>
#include <vector>

#include "FastJungle/cooker/TextureCompression.hpp"

namespace fjr::cooker {

    struct TextureCookPreparation final {
        std::vector<TextureCompressionPlan> compression_plans;
        std::uint32_t duplicate_texture_count = 0;
        std::uint32_t separated_alpha_tested_material_count = 0;
    };

    [[nodiscard]] TextureCookPreparation prepare_texture_cook(
        scene::StaticScene& scene);

} // namespace fjr::cooker
