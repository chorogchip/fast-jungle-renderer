#include "FastJungle/renderer/data/BuilderSpatial.hpp"


#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <tuple>
#include <vector>
#include <DirectXMath.h>

#include "FastJungle/core/math/Morton.hpp"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render::data {

    namespace {

        using InstanceTransform =
            DataPersistent::InstanceTransform;
        using SpatialCluster =
            DataPersistent::SpatialCluster;
        using Mesh = DataPersistent::Mesh;

        [[nodiscard]]
        std::uint32_t checked_u32(
            std::size_t value,
            const char* message) {

            if (value >
                std::numeric_limits<
                std::uint32_t>::max()) {

                log::Logger::g_logger <<
                    log::abrt(message);
            }

            return static_cast<std::uint32_t>(
                value);
        }

        template <typename T>
        void upload_buffer(
            dx::Buffer& output,
            dx::ResourceUploader& uploader,
            ID3D12Device* device,
            std::span<const T> source) {

            if (source.empty()) {
                return;
            }

            output.init(
                device,
                static_cast<UINT64>(
                    source.size_bytes()),
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_FLAG_NONE,
                D3D12_RESOURCE_STATE_COMMON);

            uploader.upload_buffer(
                output,
                std::as_bytes(source),
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }

        [[nodiscard]]
        InstanceTransform decompose_transform(
            DirectX::FXMMATRIX matrix) {

            DirectX::XMVECTOR scale;
            DirectX::XMVECTOR rotation;
            DirectX::XMVECTOR translation;

            if (!DirectX::XMMatrixDecompose(
                &scale,
                &rotation,
                &translation,
                matrix)) {

                log::Logger::g_logger << log::abrt(
                    "Scene transform cannot be represented "
                    "as TRS.");
            }

            rotation =
                DirectX::XMQuaternionNormalize(
                    rotation);

            InstanceTransform result;

            DirectX::XMStoreFloat3(
                &result.position,
                translation);

            DirectX::XMStoreFloat4(
                &result.rotation,
                rotation);

            DirectX::XMStoreFloat3(
                &result.scale,
                scale);

            return result;
        }

        [[nodiscard]]
        DirectX::XMMATRIX make_point_world(
            const scene::StaticScene::PointBatch& batch,
            const scene::StaticScene::PointInstance& instance) {

            const auto batch_local =
                DirectX::XMLoadFloat4x4(
                    &batch.local_transform);

            const auto scale =
                DirectX::XMMatrixScaling(
                    instance.scale.x,
                    instance.scale.y,
                    instance.scale.z);

            const auto rotation =
                DirectX::XMMatrixRotationQuaternion(
                    DirectX::XMLoadFloat4(
                        &instance.orientation));

            const auto translation =
                DirectX::XMMatrixTranslation(
                    instance.position.x,
                    instance.position.y,
                    instance.position.z);

            // StaticScene uses row-vector convention.
            return batch_local *
                (scale * rotation * translation);
        }

        struct Sphere {
            DirectX::XMFLOAT3 center{};
            float radius = 0.0f;
        };

        [[nodiscard]]
        Sphere transform_mesh_sphere(
            const Mesh& mesh,
            const InstanceTransform& transform) {

            const auto local_center =
                DirectX::XMLoadFloat3(
                    &mesh.bounds_center);

            const auto scale =
                DirectX::XMLoadFloat3(
                    &transform.scale);

            const auto rotation =
                DirectX::XMLoadFloat4(
                    &transform.rotation);

            const auto translation =
                DirectX::XMLoadFloat3(
                    &transform.position);

            auto center =
                DirectX::XMVectorMultiply(
                    local_center,
                    scale);

            center =
                DirectX::XMVector3Rotate(
                    center,
                    rotation);

            center =
                DirectX::XMVectorAdd(
                    center,
                    translation);

            Sphere result;

            DirectX::XMStoreFloat3(
                &result.center,
                center);

            const float maximum_scale =
                std::max({
                    std::abs(transform.scale.x),
                    std::abs(transform.scale.y),
                    std::abs(transform.scale.z),
                    });

            result.radius =
                mesh.bounds_radius *
                maximum_scale;

            return result;
        }

        class SphereAccumulator final {
        public:
            void merge(const Sphere& other) noexcept {

                if (!initialized_) {
                    sphere_ = other;
                    initialized_ = true;
                    return;
                }

                const float dx =
                    other.center.x -
                    sphere_.center.x;

                const float dy =
                    other.center.y -
                    sphere_.center.y;

                const float dz =
                    other.center.z -
                    sphere_.center.z;

                const float distance =
                    std::sqrt(
                        dx * dx +
                        dy * dy +
                        dz * dz);

                if (distance +
                    other.radius <=
                    sphere_.radius) {

                    return;
                }

                if (distance +
                    sphere_.radius <=
                    other.radius) {

                    sphere_ = other;
                    return;
                }

                if (distance <=
                    std::numeric_limits<float>::epsilon()) {

                    sphere_.radius =
                        std::max(
                            sphere_.radius,
                            other.radius);
                    return;
                }

                const float new_radius =
                    (
                        sphere_.radius +
                        distance +
                        other.radius
                        ) *
                    0.5f;

                const float movement =
                    (new_radius -
                        sphere_.radius) /
                    distance;

                sphere_.center.x +=
                    dx * movement;

                sphere_.center.y +=
                    dy * movement;

                sphere_.center.z +=
                    dz * movement;

                sphere_.radius = new_radius;
            }

            [[nodiscard]]
            Sphere get() const noexcept {
                return sphere_;
            }

        private:
            Sphere sphere_{};
            bool initialized_ = false;
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

            log::Logger::g_logger << log::abrt(
                "Invalid point category during "
                "spatial clustering.");
        }

        [[nodiscard]]
        std::uint32_t point_morton_code(
            float x,
            float z,
            std::int32_t cell_x,
            std::int32_t cell_z,
            float cell_size) noexcept {

            const auto quantize =
                [cell_size](
                    float value,
                    std::int32_t cell) noexcept {

                        const double cell_origin =
                            static_cast<double>(cell) *
                            static_cast<double>(
                                cell_size);

                        const double normalized =
                            std::clamp(
                                (
                                    static_cast<double>(
                                        value) -
                                    cell_origin
                                    ) /
                                static_cast<double>(
                                    cell_size),
                                0.0,
                                1.0);

                        return static_cast<
                            std::uint32_t>(
                                normalized *
                                65535.0 +
                                0.5);
                };

            return math::Morton::encode_2d(
                quantize(x, cell_x),
                quantize(z, cell_z));
        }

        struct PreparedPoint {
            std::uint32_t source_id = 0;

            std::int32_t cell_x = 0;
            std::int32_t cell_z = 0;

            std::uint32_t morton = 0;

            InstanceTransform transform{};
        };

        [[nodiscard]]
        PreparedPoint prepare_point(
            const scene::StaticScene::PointBatch& batch,
            const scene::StaticScene::PointInstance& source,
            std::uint32_t source_id,
            float cell_size) {

            const auto world =
                make_point_world(
                    batch,
                    source);

            PreparedPoint result;
            result.source_id = source_id;
            result.transform =
                decompose_transform(world);

            const float x =
                result.transform.position.x;
            const float z =
                result.transform.position.z;

            if (!std::isfinite(x) ||
                !std::isfinite(z)) {

                log::Logger::g_logger << log::abrt(
                    "Point instance contains "
                    "a non-finite position.");
            }

            result.cell_x =
                static_cast<std::int32_t>(
                    std::floor(
                        static_cast<double>(x) /
                        static_cast<double>(
                            cell_size)));

            result.cell_z =
                static_cast<std::int32_t>(
                    std::floor(
                        static_cast<double>(z) /
                        static_cast<double>(
                            cell_size)));

            result.morton =
                point_morton_code(
                    x,
                    z,
                    result.cell_x,
                    result.cell_z,
                    cell_size);

            return result;
        }

        void append_point_cluster(
            std::vector<InstanceTransform>& instances,
            std::vector<SpatialCluster>& clusters,
            std::span<const PreparedPoint> points,
            std::uint32_t mesh_id,
            const Mesh& mesh,
            bool impostor_probe) {

            if (points.empty()) {
                return;
            }

            SpatialCluster cluster;
            cluster.mesh_id = mesh_id;
            cluster.instance_offset =
                checked_u32(
                    instances.size(),
                    "Instance count exceeds uint32_t.");
            cluster.instance_count =
                checked_u32(
                    points.size(),
                    "Spatial cluster instance count "
                    "exceeds uint32_t.");
            cluster.impostor_probe = impostor_probe ? 1u : 0u;

            SphereAccumulator bounds;

            for (const auto& point : points) {
                instances.push_back(
                    point.transform);

                bounds.merge(
                    transform_mesh_sphere(
                        mesh,
                        point.transform));
            }

            const auto sphere =
                bounds.get();

            cluster.bounds_center =
                sphere.center;
            cluster.bounds_radius =
                sphere.radius;

            clusters.push_back(cluster);
        }

        void append_point_batches(
            std::vector<InstanceTransform>& instances,
            std::vector<SpatialCluster>& clusters,
            const scene::StaticScene& scene,
            std::span<const Mesh> meshes) {

            std::vector<PreparedPoint> points;

            for (const auto& batch :
                scene.point_batches) {

                if (batch.mesh >= meshes.size()) {
                    log::Logger::g_logger <<
                        log::abrt(
                            "PointBatch contains "
                            "an invalid mesh index.");
                }

                const std::size_t begin =
                    batch.instances.offset;

                const std::size_t count =
                    batch.instances.count;

                if (begin >
                    scene.point_instances.size() ||
                    count >
                    scene.point_instances.size() -
                    begin) {

                    log::Logger::g_logger <<
                        log::abrt(
                            "PointBatch instance range "
                            "is invalid.");
                }

                const float cell_size =
                    point_cluster_cell_size(
                        batch.category);

                points.clear();
                points.reserve(count);

                for (std::uint32_t local_id = 0;
                    local_id < batch.instances.count;
                    ++local_id) {

                    const std::uint32_t source_id =
                        batch.instances.offset +
                        local_id;

                    points.push_back(
                        prepare_point(
                            batch,
                            scene.point_instances[
                                source_id],
                                source_id,
                                cell_size));
                }

                std::sort(
                    points.begin(),
                    points.end(),
                    [](const PreparedPoint& left,
                        const PreparedPoint& right) {

                            return std::tie(
                                left.cell_x,
                                left.cell_z,
                                left.morton,
                                left.source_id) <
                                std::tie(
                                    right.cell_x,
                                    right.cell_z,
                                    right.morton,
                                    right.source_id);
                    });

                std::size_t cursor = 0;

                while (cursor < points.size()) {

                    const auto cell_x =
                        points[cursor].cell_x;

                    const auto cell_z =
                        points[cursor].cell_z;

                    std::size_t end = cursor;

                    while (
                        end < points.size() &&
                        points[end].cell_x ==
                        cell_x &&
                        points[end].cell_z ==
                        cell_z &&
                        end - cursor <
                        data::Consts::
                        PNT_CLUSTER_SZ) {

                        ++end;
                    }

                    append_point_cluster(
                        instances,
                        clusters,
                        std::span<const PreparedPoint>{
                        points.data() + cursor,
                            end - cursor,
                    },
                        batch.mesh,
                        meshes[batch.mesh],
                        batch.category ==
                            scene::StaticScene::EnumPointCategory::
                            RIVER_FOREST ||
                        batch.category ==
                            scene::StaticScene::EnumPointCategory::
                            QUEEN_FOREST);

                    cursor = end;
                }
            }
        }

        void append_static_instance(
            std::vector<InstanceTransform>& instances,
            std::vector<SpatialCluster>& clusters,
            const scene::StaticScene& scene,
            std::span<const Mesh> meshes,
            std::uint32_t instance_id) {

            if (instance_id ==
                scene::StaticScene::INVALID_INDEX) {

                return;
            }

            if (instance_id >=
                scene.static_mesh_instances.size()) {

                log::Logger::g_logger << log::abrt(
                    "Static instance index is invalid.");
            }

            const auto& source =
                scene.static_mesh_instances[
                    instance_id];

            if (source.mesh >= meshes.size()) {
                log::Logger::g_logger << log::abrt(
                    "Static instance contains "
                    "an invalid mesh index.");
            }

            const auto world =
                DirectX::XMLoadFloat4x4(
                    &source.world_transform);

            const auto transform =
                decompose_transform(world);

            SpatialCluster cluster;

            cluster.mesh_id = source.mesh;
            cluster.instance_offset =
                checked_u32(
                    instances.size(),
                    "Instance count exceeds uint32_t.");
            cluster.instance_count = 1;

            const auto sphere =
                transform_mesh_sphere(
                    meshes[source.mesh],
                    transform);

            cluster.bounds_center =
                sphere.center;
            cluster.bounds_radius =
                sphere.radius;

            instances.push_back(transform);
            clusters.push_back(cluster);
        }

        void append_static_range(
            std::vector<InstanceTransform>& instances,
            std::vector<SpatialCluster>& clusters,
            const scene::StaticScene& scene,
            std::span<const Mesh> meshes,
            scene::StaticScene::IndexRange range) {

            if (range.count == 0) {
                return;
            }

            if (range.offset >
                scene.static_mesh_instances.size() ||
                range.count >
                scene.static_mesh_instances.size() -
                range.offset) {

                log::Logger::g_logger << log::abrt(
                    "Static instance range is invalid.");
            }

            for (std::uint32_t local_id = 0;
                local_id < range.count;
                ++local_id) {

                append_static_instance(
                    instances,
                    clusters,
                    scene,
                    meshes,
                    range.offset + local_id);
            }
        }

        void append_static_instances(
            std::vector<InstanceTransform>& instances,
            std::vector<SpatialCluster>& clusters,
            const scene::StaticScene& scene,
            std::span<const Mesh> meshes) {

            const auto& components =
                scene.components;

            append_static_instance(
                instances,
                clusters,
                scene,
                meshes,
                components.pyramid.instance);

            append_static_instance(
                instances,
                clusters,
                scene,
                meshes,
                components.river.instance);

            append_static_instance(
                instances,
                clusters,
                scene,
                meshes,
                components.creek.instance);

            append_static_instance(
                instances,
                clusters,
                scene,
                meshes,
                components.banyan.instance);

            append_static_range(
                instances,
                clusters,
                scene,
                meshes,
                components.terrain.extended);

            append_static_range(
                instances,
                clusters,
                scene,
                meshes,
                components.terrain.cinematic);
        }

    } // namespace

    BuilderSpatial::Result BuilderSpatial::build(
            data::DataPersistent& output,
            dx::ResourceUploader& uploader,
            ID3D12Device* device,
            const scene::StaticScene& scene,
            std::span<const data::DataPersistent::
            Mesh> meshes) {

        if (meshes.size() != scene.meshes.size()) {
            log::Logger::g_logger << log::abrt(
                "SceneSpatialBuilder mesh table "
                "does not match StaticScene.");
        }

        std::vector<InstanceTransform>
            instance_transforms;

        std::vector<SpatialCluster>
            spatial_clusters;

        instance_transforms.reserve(
            scene.point_instances.size() +
            scene.static_mesh_instances.size());

        spatial_clusters.reserve(
            scene.point_instances.size() /
            data::Consts::PNT_CLUSTER_SZ +
            scene.point_batches.size() +
            scene.static_mesh_instances.size());

        append_point_batches(
            instance_transforms,
            spatial_clusters,
            scene,
            meshes);

        append_static_instances(
            instance_transforms,
            spatial_clusters,
            scene,
            meshes);

        upload_buffer(
            output.instance_transform,
            uploader,
            device,
            std::span<const InstanceTransform>{
            instance_transforms });

        upload_buffer(
            output.spatial_cluster,
            uploader,
            device,
            std::span<const SpatialCluster>{
            spatial_clusters });

        Result result;

        result.instance_count =
            checked_u32(
                instance_transforms.size(),
                "Instance count exceeds uint32_t.");

        result.spatial_cluster_count =
            checked_u32(
                spatial_clusters.size(),
                "Spatial cluster count exceeds uint32_t.");

        return result;
    }

} // namespace fjr::render
