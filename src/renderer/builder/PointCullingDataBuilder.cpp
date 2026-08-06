#include "FastJungle/renderer/builder/PointCullingDataBuilder.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>

#include "FastJungle/renderer/data/RenderConsts.hpp"
#include "SpacialPoint.hpp"

namespace fjr::render {

    namespace {

        [[nodiscard]]
        bool valid_range(
            scene::StaticScene::IndexRange range,
            std::size_t size) noexcept {

            return range.offset <= size &&
                range.count <= size - range.offset;
        }

        void collect_instance_information(
            data::PointCullingData& output,
            const scene::StaticScene& scene) {

            if (scene.point_instances.size() >
                std::numeric_limits<std::uint32_t>::max()) {

                throw std::overflow_error(
                    "Point instance collection is too large.");
            }

            output.instances.resize(
                scene.point_instances.size());

            output.instance_order.resize(
                scene.point_instances.size());

            std::iota(
                output.instance_order.begin(),
                output.instance_order.end(),
                0u);

            for (std::size_t batch_index = 0;
                batch_index < scene.point_mesh_batches.size();
                ++batch_index) {

                const auto& batch =
                    scene.point_mesh_batches[batch_index];

                if (!valid_range(
                    batch.category_spans,
                    scene.point_category_spans.size())) {

                    throw std::invalid_argument(
                        "Point mesh batch has an invalid category range.");
                }

                for (std::uint32_t local_span = 0;
                    local_span < batch.category_spans.count;
                    ++local_span) {

                    const auto& span =
                        scene.point_category_spans[
                            static_cast<std::size_t>(
                                batch.category_spans.offset) +
                            local_span];

                    if (!valid_range(
                        span.instances,
                        scene.point_instances.size())) {

                        throw std::invalid_argument(
                            "Point category has an invalid instance range.");
                    }

                    for (std::uint32_t local_instance = 0;
                        local_instance < span.instances.count;
                        ++local_instance) {

                        const auto source_index =
                            span.instances.offset + local_instance;

                        auto& instance =
                            output.instances[source_index];

                        if (instance.point_mesh_batch_index !=
                            scene::StaticScene::INVALID_INDEX) {

                            throw std::invalid_argument(
                                "Point instance belongs to multiple batches.");
                        }

                        instance.point_mesh_batch_index =
                            static_cast<std::uint32_t>(batch_index);
                        instance.category = span.category;
                    }
                }
            }

            if (std::ranges::any_of(
                output.instances,
                [](const data::PointCullingInstance& instance) {
                    return instance.point_mesh_batch_index ==
                        scene::StaticScene::INVALID_INDEX;
                })) {

                throw std::invalid_argument(
                    "Point batches do not cover every point instance.");
            }
        }

        void build_default_batches(
            data::PointCullingData& output) {

            for (std::size_t first = 0;
                first < output.instances.size();) {

                const auto& first_instance =
                    output.instances[first];

                std::size_t count = 1;
                while (first + count < output.instances.size() &&
                    count < data::Consts::PNT_CLUSTER_SZ) {

                    const auto& instance =
                        output.instances[first + count];

                    if (instance.point_mesh_batch_index !=
                        first_instance.point_mesh_batch_index ||
                        instance.category != first_instance.category) {

                        break;
                    }

                    ++count;
                }

                data::PointCullingBatch batch;
                batch.instances.offset =
                    static_cast<std::uint32_t>(first);
                batch.instances.count =
                    static_cast<std::uint32_t>(count);
                output.batches.push_back(batch);

                first += count;
            }
        }


        void build_custum_batches(
            data::PointCullingData& output,
            const scene::StaticScene& scene) {

            output.batches.clear();

            if (output.instance_order.size() !=
                scene.point_instances.size()) {

                throw std::logic_error(
                    "Point instance order size is inconsistent.");
            }

            output.batches.reserve(
                scene.point_instances.size() /
                data::Consts::PNT_CLUSTER_SZ +
                scene.point_category_spans.size());

            std::vector<SpatialPoint> spatial_points;

            std::size_t write_cursor = 0;

            for (const auto& mesh_batch :
                scene.point_mesh_batches) {

                for (std::uint32_t local_span = 0;
                    local_span <
                    mesh_batch.category_spans.count;
                    ++local_span) {

                    const std::size_t span_index =
                        static_cast<std::size_t>(
                            mesh_batch.category_spans.offset) +
                        local_span;

                    const auto& span =
                        scene.point_category_spans[
                            span_index];

                    if (span.instances.count == 0) {
                        continue;
                    }

                    const float cell_size =
                        point_cluster_cell_size(
                            span.category);

                    spatial_points.clear();
                    spatial_points.reserve(
                        span.instances.count);

                    for (std::uint32_t local_instance = 0;
                        local_instance <
                        span.instances.count;
                        ++local_instance) {

                        const std::uint32_t source_index =
                            span.instances.offset +
                            local_instance;

                        spatial_points.push_back(
                            make_spatial_point(
                                scene,
                                source_index,
                                cell_size));
                    }

                    std::sort(
                        spatial_points.begin(),
                        spatial_points.end(),
                        [](const SpatialPoint& left,
                            const SpatialPoint& right) {

                                return std::tie(
                                    left.cell_x,
                                    left.cell_z,
                                    left.morton,
                                    left.source_index) <
                                    std::tie(
                                        right.cell_x,
                                        right.cell_z,
                                        right.morton,
                                        right.source_index);
                        });

                    std::size_t cluster_offset =
                        write_cursor;

                    std::uint32_t cluster_count = 0;
                    std::int32_t active_cell_x = 0;
                    std::int32_t active_cell_z = 0;

                    const auto flush_cluster = [&]() {
                        if (cluster_count == 0) {
                            return;
                        }

                        data::PointCullingBatch cluster;
                        cluster.instances.offset =
                            static_cast<std::uint32_t>(
                                cluster_offset);

                        cluster.instances.count =
                            cluster_count;

                        output.batches.push_back(
                            cluster);

                        cluster_offset = write_cursor;
                        cluster_count = 0;
                        };

                    for (const auto& point :
                        spatial_points) {

                        const bool cell_changed =
                            cluster_count != 0 &&
                            (point.cell_x != active_cell_x ||
                                point.cell_z != active_cell_z);

                        const bool instance_limit_reached =
                            cluster_count >=
                            data::Consts::PNT_CLUSTER_SZ;

                        if (cell_changed ||
                            instance_limit_reached) {

                            flush_cluster();
                        }

                        if (cluster_count == 0) {
                            active_cell_x = point.cell_x;
                            active_cell_z = point.cell_z;
                            cluster_offset = write_cursor;
                        }

                        output.instance_order[
                            write_cursor] =
                            point.source_index;

                            ++write_cursor;
                            ++cluster_count;
                    }

                    flush_cluster();
                }
            }

            if (write_cursor !=
                output.instance_order.size()) {

                throw std::logic_error(
                    "Spatial clustering did not emit "
                    "every point instance.");
            }
        }

    } // namespace

    data::PointCullingData PointCullingDataBuilder::build(
            const scene::StaticScene& scene) {

        data::PointCullingData result;

        collect_instance_information(result, scene);

        // prev build function
        if (0)
            build_default_batches(result);
        else
            build_custum_batches(result, scene);

        return result;
    }

} // namespace fjr::render
