#include "FastJungle/renderer/builder/SceneBatchBuilder.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <numeric>
#include <tuple>
#include <vector>

#include "FastJungle/core/math/Morton.hpp"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render {

    namespace {

        class SpatialPoints final {
        public:
            struct SpatialPoint final {
                std::uint32_t source_index = 0;
                std::int32_t cell_x = 0;
                std::int32_t cell_z = 0;
                std::uint32_t morton = 0;
            };

            void collect(
                const scene::StaticScene& scene,
                const scene::StaticScene::PointBatch& batch) {
                const float cell_size = point_cluster_cell_size(batch.category);
                points_.clear();
                points_.reserve(batch.instances.count);
                for (std::uint32_t local_i = 0; local_i < batch.instances.count; ++local_i) {
                    const std::uint32_t source_i = batch.instances.offset + local_i;
                    points_.push_back(make(scene, source_i, cell_size));
                }
                std::sort(
                    points_.begin(), points_.end(),
                    [](const SpatialPoint& left, const SpatialPoint& right) {
                        return std::tie(
                            left.cell_x, left.cell_z, left.morton, left.source_index) <
                            std::tie(
                                right.cell_x, right.cell_z, right.morton, right.source_index);
                    });
            }

            auto begin() const noexcept { return points_.begin(); }
            auto end() const noexcept { return points_.end(); }

        private:
            [[nodiscard]]
            static float point_cluster_cell_size(
                scene::StaticScene::EnumPointCategory category) {
                using Category = scene::StaticScene::EnumPointCategory;
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
                log::Logger::g_logger << log::abrt(
                    "Invalid point category during spatial clustering.");
            }

            [[nodiscard]]
            static std::uint32_t point_morton_code(
                float x,
                float z,
                std::int32_t cell_x,
                std::int32_t cell_z,
                float cell_size) noexcept {
                const auto quantize = [cell_size](
                    float value,
                    std::int32_t cell) noexcept {
                    const double cell_origin =
                        static_cast<double>(cell) * static_cast<double>(cell_size);
                    const double normalized = std::clamp(
                        (static_cast<double>(value) - cell_origin) /
                        static_cast<double>(cell_size),
                        0.0,
                        1.0);

                    return static_cast<std::uint32_t>(
                        normalized * 65535.0 + 0.5);
                };
                return math::Morton::encode_2d(
                    quantize(x, cell_x), quantize(z, cell_z));
            }

            [[nodiscard]]
            static SpatialPoint make(
                const scene::StaticScene& scene,
                std::uint32_t source_i,
                float cell_size) {
                const auto& instance = scene.point_instances[source_i];
                if (!std::isfinite(instance.position.x) ||
                    !std::isfinite(instance.position.z)) {
                    log::Logger::g_logger << log::abrt(
                        "Point instance contains a non-finite position.");
                }
                const auto cell_x = static_cast<std::int32_t>(
                    std::floor(
                        static_cast<double>(instance.position.x) /
                        static_cast<double>(cell_size)));
                const auto cell_z = static_cast<std::int32_t>(
                    std::floor(
                        static_cast<double>(instance.position.z) /
                        static_cast<double>(cell_size)));
                return SpatialPoint{
                    .source_index = source_i,
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

            std::vector<SpatialPoint> points_;
        };

    } // namespace

    data::PointCullingData SceneBatchBuilder::build(
        const scene::StaticScene& scene) {

        data::PointCullingData result;
        result.instance_order.resize(scene.point_instances.size());
        result.batches.reserve(
            scene.point_instances.size() /
            data::Consts::PNT_CLUSTER_SZ +
            scene.point_batches.size());
        SpatialPoints spatial_points;
        std::size_t write_cursor = 0;

        for (std::size_t batch_i = 0; batch_i < scene.point_batches.size(); ++batch_i) {
            const auto& batch = scene.point_batches[batch_i];
            spatial_points.collect(scene, batch);
            std::size_t cluster_offset = write_cursor;
            std::uint32_t cluster_count = 0;
            std::int32_t active_cell_x = 0;
            std::int32_t active_cell_z = 0;
            const auto flush_cluster = [&]() {
                if (cluster_count == 0) {
                    return;
                }
                data::PointCullingBatch cluster;
                cluster.point_batch_index = static_cast<std::uint32_t>(batch_i);
                cluster.instance_order_id.offset =
                    static_cast<std::uint32_t>(cluster_offset);
                cluster.instance_order_id.count = cluster_count;
                result.batches.push_back(cluster);
                cluster_offset = write_cursor;
                cluster_count = 0;
            };
            for (const auto& point : spatial_points) {
                const bool cell_changed = cluster_count != 0 &&
                    (point.cell_x != active_cell_x || point.cell_z != active_cell_z);
                const bool instance_limit_reached =
                    cluster_count >= data::Consts::PNT_CLUSTER_SZ;
                if (cell_changed || instance_limit_reached) {
                    flush_cluster();
                }
                if (cluster_count == 0) {
                    active_cell_x = point.cell_x;
                    active_cell_z = point.cell_z;
                    cluster_offset = write_cursor;
                }
                result.instance_order[write_cursor] = point.source_index;
                ++write_cursor;
                ++cluster_count;
            }
            flush_cluster();
        }

        if (write_cursor != result.instance_order.size()) {
            log::Logger::g_logger << log::abrt(
                "Spatial clustering did not emit every point instance.");
        }
        return result;
    }

} // namespace fjr::render
