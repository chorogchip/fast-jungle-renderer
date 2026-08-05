#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::cooker {

    struct MeshLodCookSettings final {
        static constexpr std::size_t LOD_COUNT = 4;

        // Triangle targets relative to the original LOD0 topology.
        std::array<float, LOD_COUNT> triangle_ratios{
            1.0f, 0.40f, 0.15f, 0.04f};

        // Cumulative object-space error limits relative to meshopt's scale.
        std::array<float, LOD_COUNT> max_relative_errors{
            0.0f, 0.001f, 0.005f, 0.02f};

        std::uint32_t minimum_triangle_count = 128;
        float minimum_reduction = 0.20f;
    };

    struct MeshLodCookStats final {
        std::array<std::uint64_t, MeshLodCookSettings::LOD_COUNT>
            logical_index_counts{};
        std::uint64_t generated_index_count = 0;
        std::uint32_t simplified_submeshes = 0;
        std::uint32_t sloppy_fallback_submeshes = 0;
        std::uint32_t reused_submeshes = 0;
    };

    class MeshLodCooker final {
    public:
        MeshLodCooker() = delete;

        [[nodiscard]]
        static MeshLodCookStats cook(
            scene::StaticScene& scene,
            const MeshLodCookSettings& settings = {});
    };

} // namespace fjr::cooker
