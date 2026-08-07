#include "FastJungle/renderer/data/BuilderGeometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render::data {

    namespace {

        constexpr std::uint32_t MAX_SUBMESH_COUNT =
            256u * data::Consts::LOD_CNT;

        struct BoundsAccumulator {
            DirectX::XMFLOAT3 minimum{};
            DirectX::XMFLOAT3 maximum{};
            bool initialized = false;

            void merge(const DirectX::XMFLOAT3& value) noexcept {
                if (!initialized) {
                    minimum = value;
                    maximum = value;
                    initialized = true;
                    return;
                }

                minimum.x = (std::min)(minimum.x, value.x);
                minimum.y = (std::min)(minimum.y, value.y);
                minimum.z = (std::min)(minimum.z, value.z);

                maximum.x = (std::max)(maximum.x, value.x);
                maximum.y = (std::max)(maximum.y, value.y);
                maximum.z = (std::max)(maximum.z, value.z);
            }

            [[nodiscard]]
            DirectX::XMFLOAT3 center() const noexcept {
                if (!initialized) {
                    return {};
                }

                return {
                    (minimum.x + maximum.x) * 0.5f,
                    (minimum.y + maximum.y) * 0.5f,
                    (minimum.z + maximum.z) * 0.5f,
                };
            }
        };

        template <typename T>
        void upload_buffer(
            dx::Buffer& output,
            dx::ResourceUploader& uploader,
            ID3D12Device* device,
            std::span<const T> source,
            D3D12_RESOURCE_STATES final_state) {

            if (source.empty()) {
                return;
            }

            output.init(
                device,
                static_cast<UINT64>(source.size_bytes()),
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_FLAG_NONE,
                D3D12_RESOURCE_STATE_COMMON);

            uploader.upload_buffer(
                output,
                std::as_bytes(source),
                final_state);
        }

        [[nodiscard]]
        bool has_flag(
            scene::StaticScene::EnumSubmeshFlag flags,
            scene::StaticScene::EnumSubmeshFlag flag) noexcept {

            return (
                static_cast<std::uint32_t>(flags) &
                static_cast<std::uint32_t>(flag)) != 0;
        }

        [[nodiscard]]
        data::EnumRasterClass select_raster_class(
            scene::StaticScene::EnumSubmeshFlag flags) {

            const bool alpha_tested = has_flag(
                flags,
                scene::StaticScene::EnumSubmeshFlag::ALPHA_TESTED);

            const bool double_sided = has_flag(
                flags,
                scene::StaticScene::EnumSubmeshFlag::DOUBLE_SIDED);

            if (double_sided && !alpha_tested) {
                log::Logger::g_logger << log::abrt(
                    "Opaque double-sided submesh is unsupported "
                    "by current EnumRasterClass.");
            }

            return alpha_tested
                ? data::EnumRasterClass::ALPHA_TESTED_DOUBLE_SIDED
                : data::EnumRasterClass::OPAQUE_SINGLE_SIDED;
        }

        void validate_vertex_range(
            const scene::StaticScene& scene,
            const scene::StaticScene::Submesh& submesh) {

            const std::size_t begin = submesh.vertex_offset;
            const std::size_t count = submesh.vertex_count;

            if (begin > scene.vertices.size() ||
                count > scene.vertices.size() - begin) {

                log::Logger::g_logger << log::abrt(
                    "Submesh vertex range is out of bounds.");
            }
        }

        [[nodiscard]]
        DataPersistent::Mesh build_mesh(
            const scene::StaticScene& scene,
            std::size_t mesh_id) {

            const auto& source_mesh = scene.meshes[mesh_id];

            if (source_mesh.lod_count == 0 ||
                source_mesh.lod_offset >= scene.mesh_lods.size() ||
                source_mesh.lod_count >
                scene.mesh_lods.size() - source_mesh.lod_offset) {

                log::Logger::g_logger << log::abrt(
                    "Mesh LOD range is invalid.");
            }

            const auto& lod0 =
                scene.mesh_lods[source_mesh.lod_offset];

            if (lod0.submesh_offset > scene.submeshes.size() ||
                lod0.submesh_count >
                scene.submeshes.size() - lod0.submesh_offset) {

                log::Logger::g_logger << log::abrt(
                    "Mesh LOD0 submesh range is invalid.");
            }

            BoundsAccumulator bounds;

            for (std::uint32_t local_submesh = 0;
                local_submesh < lod0.submesh_count;
                ++local_submesh) {

                const auto& submesh = scene.submeshes[
                    static_cast<std::size_t>(
                        lod0.submesh_offset) +
                        local_submesh];

                validate_vertex_range(scene, submesh);

                for (std::uint32_t vertex = 0;
                    vertex < submesh.vertex_count;
                    ++vertex) {

                    bounds.merge(
                        scene.vertices[
                            static_cast<std::size_t>(
                                submesh.vertex_offset) +
                                vertex]
                        .position);
                }
            }

            DataPersistent::Mesh result;
            result.bounds_center = bounds.center();
            result.lod_offset = source_mesh.lod_offset;
            result.lod_count = source_mesh.lod_count;

            if (!bounds.initialized) {
                return result;
            }

            float radius_squared = 0.0f;

            for (std::uint32_t local_submesh = 0;
                local_submesh < lod0.submesh_count;
                ++local_submesh) {

                const auto& submesh = scene.submeshes[
                    static_cast<std::size_t>(
                        lod0.submesh_offset) +
                        local_submesh];

                for (std::uint32_t vertex = 0;
                    vertex < submesh.vertex_count;
                    ++vertex) {

                    const auto& position =
                        scene.vertices[
                            static_cast<std::size_t>(
                                submesh.vertex_offset) +
                                vertex]
                        .position;

                    const float x =
                        position.x - result.bounds_center.x;
                    const float y =
                        position.y - result.bounds_center.y;
                    const float z =
                        position.z - result.bounds_center.z;

                    radius_squared = std::max(
                        radius_squared,
                        x * x + y * y + z * z);
                }
            }

            result.bounds_radius = std::sqrt(radius_squared);
            return result;
        }

    } // namespace

    BuilderGeometry::Result BuilderGeometry::build(
        data::DataPersistent& output,
        dx::ResourceUploader& uploader,
        ID3D12Device* device,
        const scene::StaticScene& scene) {

        if (scene.submeshes.size() > MAX_SUBMESH_COUNT) {
            log::Logger::g_logger << log::abrt(
                "Scene submesh count exceeds indirect draw capacity.");
        }

        std::vector<DirectX::XMFLOAT3> positions;
        std::vector<DirectX::XMFLOAT3> normals;
        std::vector<DirectX::XMFLOAT2> uvs;

        positions.reserve(scene.vertices.size());
        normals.reserve(scene.vertices.size());
        uvs.reserve(scene.vertices.size());

        for (const auto& vertex : scene.vertices) {
            positions.push_back(vertex.position);
            normals.push_back(vertex.normal);
            uvs.push_back(vertex.uv);
        }

        upload_buffer(
            output.vertex_pos,
            uploader,
            device,
            std::span<const DirectX::XMFLOAT3>{ positions },
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

        upload_buffer(
            output.vertex_normal,
            uploader,
            device,
            std::span<const DirectX::XMFLOAT3>{ normals },
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

        upload_buffer(
            output.vertex_uv,
            uploader,
            device,
            std::span<const DirectX::XMFLOAT2>{ uvs },
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

        upload_buffer(
            output.index,
            uploader,
            device,
            std::span<const std::uint32_t>{ scene.indices },
            D3D12_RESOURCE_STATE_INDEX_BUFFER);

        std::vector<DataPersistent::SubMesh> submeshes;
        submeshes.resize(scene.submeshes.size());

        for (std::size_t index = 0;
            index < scene.submeshes.size();
            ++index) {

            const auto& source = scene.submeshes[index];

            if (source.material ==
                scene::StaticScene::INVALID_INDEX ||
                source.material >= scene.materials.size()) {

                log::Logger::g_logger << log::abrt(
                    "Submesh contains an invalid material index.");
            }

            if (source.vertex_offset >
                static_cast<std::uint32_t>(
                    std::numeric_limits<std::int32_t>::max())) {

                log::Logger::g_logger << log::abrt(
                    "Submesh base vertex exceeds int32_t.");
            }

            auto& destination = submeshes[index];

            destination.material_id = source.material;
            destination.raster_class =
                select_raster_class(source.flags);
            destination.index_offset = source.index_offset;
            destination.index_count = source.index_count;
            destination.base_vertex =
                static_cast<std::int32_t>(
                    source.vertex_offset);
        }

        std::vector<DataPersistent::MeshLod> mesh_lods;
        mesh_lods.resize(scene.mesh_lods.size());

        for (const auto& mesh : scene.meshes) {

            if (mesh.lod_count == 0 ||
                mesh.lod_offset > scene.mesh_lods.size() ||
                mesh.lod_count >
                scene.mesh_lods.size() - mesh.lod_offset) {

                log::Logger::g_logger << log::abrt(
                    "Mesh contains an invalid LOD range.");
            }

            for (std::uint32_t lod = 0;
                lod < mesh.lod_count;
                ++lod) {

                const std::size_t source_id =
                    static_cast<std::size_t>(
                        mesh.lod_offset) +
                    lod;

                const auto& source =
                    scene.mesh_lods[source_id];

                if (source.submesh_offset >
                    scene.submeshes.size() ||
                    source.submesh_count >
                    scene.submeshes.size() -
                    source.submesh_offset) {

                    log::Logger::g_logger << log::abrt(
                        "MeshLod contains an invalid "
                        "submesh range.");
                }

                auto& destination = mesh_lods[source_id];

                destination.submesh_offset =
                    source.submesh_offset;
                destination.submesh_count =
                    source.submesh_count;
                destination.lod_error =
                    source.max_deviation;

                destination.next_lod_error =
                    lod + 1 < mesh.lod_count
                    ? scene.mesh_lods[
                        source_id + 1].max_deviation
                    : std::numeric_limits<float>::infinity();
            }
        }

        Result result;
        result.meshes.resize(scene.meshes.size());

        for (std::size_t mesh_id = 0;
            mesh_id < scene.meshes.size();
            ++mesh_id) {

            result.meshes[mesh_id] =
                build_mesh(scene, mesh_id);
        }

        upload_buffer(
            output.submesh,
            uploader,
            device,
            std::span<const DataPersistent::SubMesh>{ submeshes },
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        upload_buffer(
            output.mesh_lod,
            uploader,
            device,
            std::span<const DataPersistent::MeshLod>{ mesh_lods },
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        upload_buffer(
            output.mesh,
            uploader,
            device,
            std::span<const DataPersistent::Mesh>{ result.meshes },
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        return result;
    }

} // namespace fjr::render