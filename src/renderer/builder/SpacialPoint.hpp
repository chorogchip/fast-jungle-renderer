#pragma once

namespace fjr::render {

    struct SpatialPoint final {
        std::uint32_t source_index = 0;
        std::int32_t cell_x = 0;
        std::int32_t cell_z = 0;
        std::uint32_t morton = 0;
    };

    [[nodiscard]]
    float point_cluster_cell_size(
        scene::StaticScene::EnumPointCategory category) {

        using Category =
            scene::StaticScene::EnumPointCategory;

        switch (category) {
        case Category::RIVER_FOREST:
        case Category::QUEEN_FOREST:
            return 128.0f;

        case Category::RIVER_SEEDLING:
        case Category::RIVER_SAPLING:
            return 64.0f;

        case Category::PYRAMID_MOSS:
        case Category::PYRAMID_GRASS_B:
            return 32.0f;

        case Category::ANTHURIUM:
        case Category::NETTLE:
        case Category::SHRUB_SORREL:
        case Category::SHRUB:
        case Category::GRASS_B:
        case Category::GRASS_A:
            return 128.0f;

        case Category::COUNT:
            break;
        }

        throw std::invalid_argument(
            "Invalid point category during spatial clustering.");
    }

    [[nodiscard]]
    std::uint32_t point_morton_code(
        float x,
        float z,
        std::int32_t cell_x,
        std::int32_t cell_z,
        float cell_size) noexcept {

        const auto quantize = [cell_size](
            float value,
            std::int32_t cell) noexcept {

                const double cell_origin =
                    static_cast<double>(cell) *
                    static_cast<double>(cell_size);

                const double normalized = std::clamp(
                    (static_cast<double>(value) - cell_origin) /
                    static_cast<double>(cell_size),
                    0.0,
                    1.0);

                return static_cast<std::uint32_t>(
                    normalized * 65535.0 + 0.5);
            };

        const auto spread_bits = [](
            std::uint32_t value) noexcept {

                value &= 0x0000ffffu;
                value = (value | (value << 8u)) &
                    0x00ff00ffu;
                value = (value | (value << 4u)) &
                    0x0f0f0f0fu;
                value = (value | (value << 2u)) &
                    0x33333333u;
                value = (value | (value << 1u)) &
                    0x55555555u;

                return value;
            };

        const std::uint32_t quantized_x =
            quantize(x, cell_x);

        const std::uint32_t quantized_z =
            quantize(z, cell_z);

        return spread_bits(quantized_x) |
            (spread_bits(quantized_z) << 1u);
    }

    [[nodiscard]]
    SpatialPoint make_spatial_point(
        const scene::StaticScene& scene,
        std::uint32_t source_index,
        float cell_size) {

        const auto& instance =
            scene.point_instances[source_index];

        if (!std::isfinite(instance.position.x) ||
            !std::isfinite(instance.position.z)) {

            throw std::invalid_argument(
                "Point instance contains a non-finite position.");
        }

        const auto cell_x =
            static_cast<std::int32_t>(
                std::floor(
                    static_cast<double>(instance.position.x) /
                    static_cast<double>(cell_size)));

        const auto cell_z =
            static_cast<std::int32_t>(
                std::floor(
                    static_cast<double>(instance.position.z) /
                    static_cast<double>(cell_size)));

        return SpatialPoint{
            .source_index = source_index,
            .cell_x = cell_x,
            .cell_z = cell_z,
            .morton = point_morton_code(
                instance.position.x,
                instance.position.z,
                cell_x,
                cell_z,
                cell_size),
        };
    }
}