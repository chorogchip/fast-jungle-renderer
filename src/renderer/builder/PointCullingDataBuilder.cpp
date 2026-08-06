#include "FastJungle/renderer/builder/PointCullingDataBuilder.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>

#include "FastJungle/renderer/data/RenderConsts.hpp"

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

        void validate_user_result(
            const data::PointCullingData& result,
            const scene::StaticScene& scene) {

            if (result.instance_order.size() >
                std::numeric_limits<std::uint32_t>::max()) {

                throw std::overflow_error(
                    "Point culling instance order is too large.");
            }

            for (const auto source_index : result.instance_order) {
                if (source_index >= scene.point_instances.size()) {
                    throw std::out_of_range(
                        "Point culling order contains an invalid source index.");
                }
            }

            for (const auto& batch : result.batches) {
                if (batch.instances.count == 0 ||
                    !valid_range(
                        batch.instances,
                        result.instance_order.size())) {

                    throw std::invalid_argument(
                        "Point culling batch has an invalid instance range.");
                }

                const auto first_source =
                    result.instance_order[batch.instances.offset];
                const auto& first = result.instances[first_source];

                for (std::uint32_t local_instance = 1;
                    local_instance < batch.instances.count;
                    ++local_instance) {

                    const auto source_index =
                        result.instance_order[
                            static_cast<std::size_t>(
                                batch.instances.offset) +
                            local_instance];
                    const auto& instance =
                        result.instances[source_index];

                    if (instance.point_mesh_batch_index !=
                        first.point_mesh_batch_index ||
                        instance.category != first.category) {

                        throw std::invalid_argument(
                            "A point culling batch mixes meshes or categories.");
                    }
                }
            }
        }

    } // namespace

    data::PointCullingData
        PointCullingDataBuilder::build(
            const scene::StaticScene& scene,
            data::PointCullingBuildFunction user_build_function) {

        data::PointCullingData result;
        collect_instance_information(result, scene);

        if (user_build_function == nullptr) {
            build_default_batches(result);
        } else {
            data::PointCullingBuildContext context{
                scene,
                result.instances,
                result.instance_order,
                result.batches,
            };
            user_build_function(context);
        }

        validate_user_result(result, scene);
        return result;
    }

} // namespace fjr::render
