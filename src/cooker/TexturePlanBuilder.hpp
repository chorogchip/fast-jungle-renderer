#pragma once

#include <vector>

#include "TextureCompression.hpp"

namespace fjr::cooker {

    class TexturePlanBuilder final {
    public:
        TexturePlanBuilder() = delete;

        [[nodiscard]] static std::vector<TextureCompressionPlan> build(
            scene::StaticScene& scene);
    };

} // namespace fjr::cooker
