#include "FastJungle/renderer/Renderer.hpp"

#include "RendererMaterial.hpp"

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fjr {

    namespace {

        using Scene = cooker::JungleScene;
        using ObjectKind = Scene::ObjectKind;

        // Temporary renderer options. Change one kind and set
        // RENDER_ALL_OBJECT_KINDS to false to inspect a single object family.
        constexpr bool RENDER_ALL_OBJECT_KINDS = true;
        constexpr ObjectKind RENDER_OBJECT_KIND = ObjectKind::Anthurium;
        constexpr std::uint32_t MAX_INSTANCERS_PER_KIND = 1;
        constexpr std::uint32_t MAX_INSTANCES_PER_INSTANCER = 1;

        constexpr std::uint32_t GALLERY_COLUMNS = 5;
        constexpr float GALLERY_CELL_SIZE = 2.4f;
        constexpr float GALLERY_OBJECT_SIZE = 1.7f;

        constexpr std::array RENDERABLE_OBJECT_KINDS{
            ObjectKind::Anthurium,
            ObjectKind::GrassA,
            ObjectKind::GrassB,
            ObjectKind::PyramidGrassB,
            ObjectKind::PyramidMoss,
            ObjectKind::QueenForest,
            ObjectKind::RiverForest,
            ObjectKind::RiverSapling,
            ObjectKind::RiverSeedling,
            ObjectKind::Shrub,
            ObjectKind::ShrubSorrel,
            ObjectKind::Nettle,
            ObjectKind::Terrain,
            ObjectKind::Pyramid,
            ObjectKind::Banyan,
            ObjectKind::River,
            ObjectKind::Creek,
        };

        constexpr std::array POINT_INSTANCED_KINDS{
            ObjectKind::Anthurium,
            ObjectKind::GrassA,
            ObjectKind::GrassB,
            ObjectKind::PyramidGrassB,
            ObjectKind::PyramidMoss,
            ObjectKind::QueenForest,
            ObjectKind::RiverForest,
            ObjectKind::RiverSapling,
            ObjectKind::RiverSeedling,
            ObjectKind::Shrub,
            ObjectKind::ShrubSorrel,
            ObjectKind::Nettle,
        };

        struct Bounds {
            DirectX::XMFLOAT3 minimum{
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max()
            };
            DirectX::XMFLOAT3 maximum{
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest()
            };
            bool valid = false;

            void include(DirectX::FXMVECTOR point) {
                DirectX::XMFLOAT3 value;
                DirectX::XMStoreFloat3(&value, point);
                if (!std::isfinite(value.x) ||
                    !std::isfinite(value.y) ||
                    !std::isfinite(value.z)) {
                    throw std::runtime_error(
                        "Jungle geometry contains a non-finite position.");
                }
                minimum.x = std::min(minimum.x, value.x);
                minimum.y = std::min(minimum.y, value.y);
                minimum.z = std::min(minimum.z, value.z);
                maximum.x = std::max(maximum.x, value.x);
                maximum.y = std::max(maximum.y, value.y);
                maximum.z = std::max(maximum.z, value.z);
                valid = true;
            }

            void include(const Bounds& other) {
                if (!other.valid) {
                    return;
                }
                include(DirectX::XMLoadFloat3(&other.minimum));
                include(DirectX::XMLoadFloat3(&other.maximum));
            }
        };

        struct CpuBatch {
            std::uint32_t mesh_index = Scene::INVALID_INDEX;
            std::vector<DirectX::XMFLOAT4X4> transforms;
        };

        struct CpuObject {
            ObjectKind kind = ObjectKind::Unknown;
            std::vector<CpuBatch> batches;
            Bounds bounds;
        };

        struct PrototypeMesh {
            std::uint32_t mesh_index = Scene::INVALID_INDEX;
            DirectX::XMFLOAT4X4 transform{};
        };

        struct Vertex {
            DirectX::XMFLOAT3 position{};
            DirectX::XMFLOAT3 normal{};
            DirectX::XMFLOAT4 tangent{};
            DirectX::XMFLOAT2 uv{};
        };

        struct GpuInstance {
            std::array<DirectX::XMFLOAT4, 7> rows{};
        };

        struct FaceGroup {
            std::string material_path;
            std::vector<std::uint32_t> faces;
        };

        struct GeometryPart {
            std::vector<Vertex> vertices;
            std::vector<std::uint32_t> indices;
        };

        void throw_if_failed(HRESULT result, const char* operation) {
            if (FAILED(result)) {
                throw std::runtime_error(operation);
            }
        }

        DirectX::XMMATRIX load_matrix(const Scene::Matrix4x4& source) {
            DirectX::XMFLOAT4X4 result;
            for (std::size_t row = 0; row < 4; ++row) {
                for (std::size_t column = 0; column < 4; ++column) {
                    result.m[row][column] = static_cast<float>(
                        source.values[row * 4 + column]);
                }
            }
            return DirectX::XMLoadFloat4x4(&result);
        }

        DirectX::XMFLOAT4X4 store_matrix(DirectX::FXMMATRIX source) {
            DirectX::XMFLOAT4X4 result;
            DirectX::XMStoreFloat4x4(&result, source);
            return result;
        }

        bool is_point_instanced_kind(ObjectKind kind) {
            return std::ranges::find(POINT_INSTANCED_KINDS, kind) !=
                POINT_INSTANCED_KINDS.end();
        }

        bool should_render_kind(ObjectKind kind) {
            return RENDER_ALL_OBJECT_KINDS || kind == RENDER_OBJECT_KIND;
        }

        std::vector<FaceGroup> build_face_groups(
            const Scene& scene,
            const Scene::Mesh& mesh) {

            std::vector<std::string> face_materials(
                mesh.face_vertex_counts.size(),
                mesh.material_path);
            for (const auto& subset : scene.mesh_subsets) {
                if (subset.mesh_path != mesh.prim_path) {
                    continue;
                }
                if (subset.element_type != "face") {
                    throw std::runtime_error(
                        "Non-face material subset in " + subset.prim_path);
                }
                if (subset.material_path.empty()) {
                    throw std::runtime_error(
                        "Unbound material subset in " + subset.prim_path);
                }
                for (const auto signed_face : subset.indices) {
                    if (signed_face < 0 ||
                        static_cast<std::size_t>(signed_face) >=
                            face_materials.size()) {
                        throw std::runtime_error(
                            "Invalid material subset face in " +
                            subset.prim_path);
                    }
                    auto& material = face_materials[
                        static_cast<std::size_t>(signed_face)];
                    if (material != mesh.material_path &&
                        material != subset.material_path) {
                        throw std::runtime_error(
                            "Overlapping Jungle material subsets in " +
                            mesh.prim_path);
                    }
                    material = subset.material_path;
                }
            }

            std::vector<FaceGroup> result;
            std::unordered_map<std::string, std::size_t> group_indices;
            for (std::uint32_t face = 0;
                face < face_materials.size();
                ++face) {
                const auto& material = face_materials[face];
                if (material.empty()) {
                    throw std::runtime_error(
                        "Unbound Jungle mesh face in " + mesh.prim_path);
                }
                const auto [iterator, inserted] = group_indices.emplace(
                    material,
                    result.size());
                if (inserted) {
                    result.push_back({material, {}});
                }
                result[iterator->second].faces.push_back(face);
            }
            return result;
        }

        std::size_t interpolation_index(
            std::string_view interpolation,
            std::size_t face,
            std::size_t corner,
            std::uint32_t point) {

            if (interpolation.empty() || interpolation == "constant") {
                return 0;
            }
            if (interpolation == "uniform") {
                return face;
            }
            if (interpolation == "vertex" || interpolation == "varying") {
                return point;
            }
            if (interpolation == "faceVarying") {
                return corner;
            }
            throw std::runtime_error(
                "Unsupported Jungle interpolation " +
                std::string{interpolation});
        }

        const Scene::Primvar& find_uv_primvar(
            const Scene::Mesh& mesh,
            std::string_view name) {

            const auto primvar = std::ranges::find(
                mesh.primvars,
                name,
                &Scene::Primvar::name);
            if (primvar == mesh.primvars.end() ||
                primvar->storage != Scene::PrimvarStorage::Float2) {
                throw std::runtime_error(
                    "Missing float2 primvar " + std::string{name} +
                    " on " + mesh.prim_path);
            }
            return *primvar;
        }

        Scene::Float2 read_uv(
            const Scene::Mesh& mesh,
            const Scene::Primvar& primvar,
            std::size_t face,
            std::size_t corner,
            std::uint32_t point) {

            const auto source_index = interpolation_index(
                primvar.interpolation,
                face,
                corner,
                point);
            std::int64_t value_index = static_cast<std::int64_t>(source_index);
            if (!primvar.indices.empty()) {
                if (source_index >= primvar.indices.size()) {
                    throw std::runtime_error(
                        "UV index stream is too small on " + mesh.prim_path);
                }
                value_index = primvar.indices[source_index];
            }
            const auto& values = std::get<std::vector<Scene::Float2>>(
                primvar.data);
            if (value_index < 0 ||
                static_cast<std::size_t>(value_index) >= values.size()) {
                throw std::runtime_error(
                    "UV value index is invalid on " + mesh.prim_path);
            }
            return values[static_cast<std::size_t>(value_index)];
        }

        DirectX::XMVECTOR read_normal(
            const Scene::Mesh& mesh,
            std::size_t face,
            std::size_t corner,
            std::uint32_t point) {

            if (mesh.normals.empty()) {
                return DirectX::XMVectorZero();
            }
            const auto index = interpolation_index(
                mesh.normals_interpolation,
                face,
                corner,
                point);
            if (index >= mesh.normals.size()) {
                throw std::runtime_error(
                    "Normal index is invalid on " + mesh.prim_path);
            }
            const auto& normal = mesh.normals[index];
            return DirectX::XMVectorSet(normal.x, normal.y, normal.z, 0.0f);
        }

        void calculate_tangents(std::array<Vertex, 3>& triangle) {
            const auto p0 = DirectX::XMLoadFloat3(&triangle[0].position);
            const auto p1 = DirectX::XMLoadFloat3(&triangle[1].position);
            const auto p2 = DirectX::XMLoadFloat3(&triangle[2].position);
            const auto edge1 = DirectX::XMVectorSubtract(p1, p0);
            const auto edge2 = DirectX::XMVectorSubtract(p2, p0);
            const float du1 = triangle[1].uv.x - triangle[0].uv.x;
            const float dv1 = triangle[1].uv.y - triangle[0].uv.y;
            const float du2 = triangle[2].uv.x - triangle[0].uv.x;
            const float dv2 = triangle[2].uv.y - triangle[0].uv.y;
            const float determinant = du1 * dv2 - du2 * dv1;

            DirectX::XMVECTOR tangent;
            DirectX::XMVECTOR bitangent;
            if (std::abs(determinant) > 1.0e-8f) {
                const float inverse = 1.0f / determinant;
                tangent = DirectX::XMVectorScale(
                    DirectX::XMVectorSubtract(
                        DirectX::XMVectorScale(edge1, dv2),
                        DirectX::XMVectorScale(edge2, dv1)),
                    inverse);
                bitangent = DirectX::XMVectorScale(
                    DirectX::XMVectorSubtract(
                        DirectX::XMVectorScale(edge2, du1),
                        DirectX::XMVectorScale(edge1, du2)),
                    inverse);
            }
            else {
                tangent = edge1;
                bitangent = edge2;
            }

            for (auto& vertex : triangle) {
                auto normal = DirectX::XMLoadFloat3(&vertex.normal);
                auto orthogonal = DirectX::XMVectorSubtract(
                    tangent,
                    DirectX::XMVectorScale(
                        normal,
                        DirectX::XMVectorGetX(
                            DirectX::XMVector3Dot(normal, tangent))));
                if (DirectX::XMVectorGetX(
                    DirectX::XMVector3LengthSq(orthogonal)) < 1.0e-10f) {
                    const auto axis = std::abs(vertex.normal.z) < 0.9f
                        ? DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)
                        : DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
                    orthogonal = DirectX::XMVector3Cross(axis, normal);
                }
                orthogonal = DirectX::XMVector3Normalize(orthogonal);
                const float sign = DirectX::XMVectorGetX(
                    DirectX::XMVector3Dot(
                        DirectX::XMVector3Cross(normal, orthogonal),
                        bitangent)) < 0.0f ? -1.0f : 1.0f;
                DirectX::XMStoreFloat4(
                    &vertex.tangent,
                    DirectX::XMVectorSetW(orthogonal, sign));
            }
        }

        GeometryPart build_geometry_part(
            const Scene::Mesh& mesh,
            const FaceGroup& group,
            std::string_view uv_primvar_name) {

            const auto& uv_primvar = find_uv_primvar(
                mesh,
                uv_primvar_name);
            std::vector<std::size_t> face_corners(
                mesh.face_vertex_counts.size());
            std::size_t corner_count = 0;
            for (std::size_t face = 0;
                face < mesh.face_vertex_counts.size();
                ++face) {
                face_corners[face] = corner_count;
                const auto count = mesh.face_vertex_counts[face];
                if (count < 0) {
                    throw std::runtime_error(
                        "Negative face size in " + mesh.prim_path);
                }
                corner_count += static_cast<std::size_t>(count);
            }
            if (corner_count != mesh.face_vertex_indices.size()) {
                throw std::runtime_error(
                    "Face data does not match index data in " +
                    mesh.prim_path);
            }

            const std::unordered_set<std::int32_t> holes{
                mesh.hole_indices.begin(),
                mesh.hole_indices.end()
            };
            GeometryPart result;
            for (const auto face : group.faces) {
                if (holes.contains(static_cast<std::int32_t>(face))) {
                    continue;
                }
                const auto count = static_cast<std::size_t>(
                    mesh.face_vertex_counts[face]);
                if (count < 3) {
                    continue;
                }
                const auto first = face_corners[face];
                for (std::size_t triangle_index = 1;
                    triangle_index + 1 < count;
                    ++triangle_index) {
                    std::array<std::size_t, 3> corners{
                        first,
                        first + triangle_index,
                        first + triangle_index + 1
                    };
                    if (mesh.orientation == "leftHanded") {
                        std::swap(corners[1], corners[2]);
                    }

                    std::array<Vertex, 3> triangle;
                    for (std::size_t vertex_index = 0;
                        vertex_index < triangle.size();
                        ++vertex_index) {
                        const auto corner = corners[vertex_index];
                        const auto signed_point =
                            mesh.face_vertex_indices[corner];
                        if (signed_point < 0 ||
                            static_cast<std::size_t>(signed_point) >=
                                mesh.points.size()) {
                            throw std::runtime_error(
                                "Invalid point index in " + mesh.prim_path);
                        }
                        const auto point = static_cast<std::uint32_t>(
                            signed_point);
                        const auto& position = mesh.points[point];
                        triangle[vertex_index].position = {
                            position.x,
                            position.y,
                            position.z
                        };
                        const auto uv = read_uv(
                            mesh,
                            uv_primvar,
                            face,
                            corner,
                            point);
                        triangle[vertex_index].uv = {uv.x, uv.y};
                        const auto normal = read_normal(
                            mesh,
                            face,
                            corner,
                            point);
                        if (DirectX::XMVectorGetX(
                            DirectX::XMVector3LengthSq(normal)) >= 1.0e-10f) {
                            DirectX::XMStoreFloat3(
                                &triangle[vertex_index].normal,
                                DirectX::XMVector3Normalize(normal));
                        }
                    }

                    const auto p0 = DirectX::XMLoadFloat3(
                        &triangle[0].position);
                    const auto p1 = DirectX::XMLoadFloat3(
                        &triangle[1].position);
                    const auto p2 = DirectX::XMLoadFloat3(
                        &triangle[2].position);
                    const auto face_normal = DirectX::XMVector3Normalize(
                        DirectX::XMVector3Cross(
                            DirectX::XMVectorSubtract(p1, p0),
                            DirectX::XMVectorSubtract(p2, p0)));
                    for (auto& vertex : triangle) {
                        if (DirectX::XMVectorGetX(
                            DirectX::XMVector3LengthSq(
                                DirectX::XMLoadFloat3(&vertex.normal))) <
                                1.0e-10f) {
                            DirectX::XMStoreFloat3(
                                &vertex.normal,
                                face_normal);
                        }
                    }
                    calculate_tangents(triangle);

                    for (const auto& vertex : triangle) {
                        if (result.vertices.size() >=
                            std::numeric_limits<std::uint32_t>::max()) {
                            throw std::runtime_error(
                                "Expanded Jungle mesh is too large.");
                        }
                        result.indices.push_back(
                            static_cast<std::uint32_t>(
                                result.vertices.size()));
                        result.vertices.push_back(vertex);
                    }
                }
            }
            return result;
        }

        GpuInstance make_gpu_instance(DirectX::FXMMATRIX transform) {
            GpuInstance result;
            const auto normal_transform = DirectX::XMMatrixTranspose(
                DirectX::XMMatrixInverse(nullptr, transform));
            DirectX::XMFLOAT4X4 stored_transform;
            DirectX::XMFLOAT4X4 stored_normal;
            DirectX::XMStoreFloat4x4(&stored_transform, transform);
            DirectX::XMStoreFloat4x4(&stored_normal, normal_transform);
            for (std::size_t row = 0; row < 4; ++row) {
                result.rows[row] = {
                    stored_transform.m[row][0],
                    stored_transform.m[row][1],
                    stored_transform.m[row][2],
                    stored_transform.m[row][3]
                };
            }
            for (std::size_t row = 0; row < 3; ++row) {
                result.rows[row + 4] = {
                    stored_normal.m[row][0],
                    stored_normal.m[row][1],
                    stored_normal.m[row][2],
                    0.0f
                };
            }
            return result;
        }

        void upload_bytes(
            dx::Buffer& destination,
            ID3D12Device* device,
            const void* source,
            std::size_t byte_size) {

            if (byte_size == 0) {
                throw std::runtime_error("Cannot upload an empty buffer.");
            }
            destination.init(
                device,
                static_cast<UINT64>(byte_size),
                D3D12_HEAP_TYPE_UPLOAD,
                D3D12_RESOURCE_FLAG_NONE,
                D3D12_RESOURCE_STATE_GENERIC_READ);

            void* mapped = nullptr;
            constexpr D3D12_RANGE read_range{0, 0};
            throw_if_failed(
                destination->Map(0, &read_range, &mapped),
                "ID3D12Resource::Map failed.");
            std::memcpy(mapped, source, byte_size);
            const D3D12_RANGE written_range{0, byte_size};
            destination->Unmap(0, &written_range);
        }

        template<typename T>
        void upload_vector(
            dx::Buffer& destination,
            ID3D12Device* device,
            const std::vector<T>& source) {

            upload_bytes(
                destination,
                device,
                source.data(),
                source.size() * sizeof(T));
        }

        std::vector<std::uint32_t> build_mesh_node_map(
            const Scene& scene) {

            std::vector<std::uint32_t> result(
                scene.meshes.size(),
                Scene::INVALID_INDEX);
            for (std::uint32_t node_index = 0;
                node_index < scene.nodes.size();
                ++node_index) {

                const auto& node = scene.nodes[node_index];
                if (node.prim_kind == Scene::PrimKind::Mesh &&
                    node.payload < result.size()) {
                    result[node.payload] = node_index;
                }
            }
            return result;
        }

        DirectX::XMMATRIX node_world_matrix(
            const Scene& scene,
            std::uint32_t node_index,
            std::vector<DirectX::XMFLOAT4X4>& cache,
            std::vector<std::uint8_t>& states) {

            if (node_index >= scene.nodes.size()) {
                return DirectX::XMMatrixIdentity();
            }
            if (states[node_index] == 2) {
                return DirectX::XMLoadFloat4x4(&cache[node_index]);
            }
            if (states[node_index] == 1) {
                throw std::runtime_error("Cycle in Jungle node hierarchy.");
            }
            states[node_index] = 1;
            const auto& node = scene.nodes[node_index];
            auto result = load_matrix(node.local_transform);
            if (node.parent != Scene::INVALID_INDEX) {
                result = DirectX::XMMatrixMultiply(
                    result,
                    node_world_matrix(
                        scene,
                        node.parent,
                        cache,
                        states));
            }
            cache[node_index] = store_matrix(result);
            states[node_index] = 2;
            return result;
        }

        bool is_descendant(
            const Scene& scene,
            std::uint32_t node_index,
            std::uint32_t ancestor_index) {

            auto current = node_index;
            while (current != Scene::INVALID_INDEX &&
                current < scene.nodes.size()) {
                if (current == ancestor_index) {
                    return true;
                }
                current = scene.nodes[current].parent;
            }
            return false;
        }

        DirectX::XMMATRIX matrix_to_ancestor(
            const Scene& scene,
            std::uint32_t node_index,
            std::uint32_t ancestor_index) {

            auto result = DirectX::XMMatrixIdentity();
            auto current = node_index;
            while (current != Scene::INVALID_INDEX &&
                current < scene.nodes.size()) {
                result = DirectX::XMMatrixMultiply(
                    result,
                    load_matrix(scene.nodes[current].local_transform));
                if (current == ancestor_index) {
                    return result;
                }
                current = scene.nodes[current].parent;
            }
            throw std::runtime_error(
                "Prototype mesh is outside its prototype hierarchy.");
        }

        std::vector<PrototypeMesh> prototype_meshes(
            const Scene& scene,
            std::uint32_t prototype_node,
            const std::vector<std::uint32_t>& mesh_nodes,
            const std::unordered_map<std::string, std::uint32_t>& node_indices) {

            std::uint32_t geometry_root = prototype_node;
            auto root_transform = DirectX::XMMatrixIdentity();
            const auto& prototype = scene.nodes[prototype_node];
            if ((prototype.flags & Scene::NodeNativeInstance) != 0 &&
                !prototype.native_prototype_path.empty()) {
                const auto native = node_indices.find(
                    prototype.native_prototype_path);
                if (native == node_indices.end()) {
                    throw std::runtime_error(
                        "Missing native prototype " +
                        prototype.native_prototype_path);
                }
                geometry_root = native->second;
                root_transform = load_matrix(prototype.local_transform);
            }

            std::vector<PrototypeMesh> result;
            for (std::uint32_t mesh_index = 0;
                mesh_index < mesh_nodes.size();
                ++mesh_index) {
                const auto mesh_node = mesh_nodes[mesh_index];
                if (mesh_node == Scene::INVALID_INDEX ||
                    !is_descendant(scene, mesh_node, geometry_root)) {
                    continue;
                }
                const auto transform = DirectX::XMMatrixMultiply(
                    matrix_to_ancestor(scene, mesh_node, geometry_root),
                    root_transform);
                result.push_back({mesh_index, store_matrix(transform)});
            }
            return result;
        }

        DirectX::XMMATRIX point_instance_matrix(
            const Scene::PointInstancer& instancer,
            std::size_t instance_index,
            DirectX::FXMMATRIX instancer_world) {

            const auto& scale = instancer.scales[instance_index];
            const auto& orientation = instancer.orientations[instance_index];
            const auto& position = instancer.positions[instance_index];
            auto quaternion = DirectX::XMVectorSet(
                orientation.imaginary.x,
                orientation.imaginary.y,
                orientation.imaginary.z,
                orientation.real);
            if (DirectX::XMVectorGetX(
                DirectX::XMVector4LengthSq(quaternion)) < 1.0e-12f) {
                quaternion = DirectX::XMQuaternionIdentity();
            }
            else {
                quaternion = DirectX::XMQuaternionNormalize(quaternion);
            }
            auto result = DirectX::XMMatrixScaling(
                scale.x,
                scale.y,
                scale.z);
            result = DirectX::XMMatrixMultiply(
                result,
                DirectX::XMMatrixRotationQuaternion(quaternion));
            result = DirectX::XMMatrixMultiply(
                result,
                DirectX::XMMatrixTranslation(
                    position.x,
                    position.y,
                    position.z));
            return DirectX::XMMatrixMultiply(result, instancer_world);
        }

        void include_batch_bounds(
            CpuObject& object,
            const Scene& scene,
            const CpuBatch& batch) {

            const auto& mesh = scene.meshes[batch.mesh_index];
            for (const auto& stored_transform : batch.transforms) {
                const auto transform = DirectX::XMLoadFloat4x4(
                    &stored_transform);
                for (const auto& point : mesh.points) {
                    object.bounds.include(DirectX::XMVector3TransformCoord(
                        DirectX::XMVectorSet(
                            point.x,
                            point.y,
                            point.z,
                            1.0f),
                        transform));
                }
            }
        }

        CpuObject build_point_object(
            const Scene& scene,
            ObjectKind kind,
            const std::unordered_map<std::string, std::uint32_t>& node_indices,
            const std::vector<std::uint32_t>& mesh_nodes,
            std::vector<DirectX::XMFLOAT4X4>& world_cache,
            std::vector<std::uint8_t>& world_states) {

            CpuObject result;
            result.kind = kind;
            std::uint32_t imported_instancers = 0;

            for (const auto& instancer : scene.point_instancers) {
                if (instancer.object_kind != kind ||
                    imported_instancers >= MAX_INSTANCERS_PER_KIND) {
                    continue;
                }
                const auto instancer_node = node_indices.find(
                    instancer.prim_path);
                if (instancer_node == node_indices.end()) {
                    throw std::runtime_error(
                        "Missing PointInstancer node " +
                        instancer.prim_path);
                }
                const auto available_instances = std::min({
                    instancer.positions.size(),
                    instancer.orientations.size(),
                    instancer.scales.size(),
                    instancer.prototype_indices.size()
                });
                const auto selected_instances = std::min<std::size_t>(
                    available_instances,
                    MAX_INSTANCES_PER_INSTANCER);
                if (selected_instances == 0) {
                    continue;
                }

                std::unordered_map<
                    std::int32_t,
                    std::vector<DirectX::XMFLOAT4X4>> transforms_by_prototype;
                const auto instancer_world = node_world_matrix(
                    scene,
                    instancer_node->second,
                    world_cache,
                    world_states);
                for (std::size_t instance = 0;
                    instance < selected_instances;
                    ++instance) {
                    const auto prototype_index =
                        instancer.prototype_indices[instance];
                    if (prototype_index < 0 ||
                        static_cast<std::size_t>(prototype_index) >=
                            instancer.prototype_paths.size()) {
                        continue;
                    }
                    transforms_by_prototype[prototype_index].push_back(
                        store_matrix(point_instance_matrix(
                            instancer,
                            instance,
                            instancer_world)));
                }

                for (auto& [prototype_index, instance_transforms] :
                    transforms_by_prototype) {
                    const auto& prototype_path = instancer.prototype_paths[
                        static_cast<std::size_t>(prototype_index)];
                    const auto prototype_node = node_indices.find(
                        prototype_path);
                    if (prototype_node == node_indices.end()) {
                        throw std::runtime_error(
                            "Missing point prototype " + prototype_path);
                    }
                    const auto meshes = prototype_meshes(
                        scene,
                        prototype_node->second,
                        mesh_nodes,
                        node_indices);
                    for (const auto& prototype_mesh : meshes) {
                        CpuBatch batch;
                        batch.mesh_index = prototype_mesh.mesh_index;
                        const auto mesh_transform =
                            DirectX::XMLoadFloat4x4(
                                &prototype_mesh.transform);
                        batch.transforms.reserve(instance_transforms.size());
                        for (const auto& stored_instance :
                            instance_transforms) {
                            batch.transforms.push_back(store_matrix(
                                DirectX::XMMatrixMultiply(
                                    mesh_transform,
                                    DirectX::XMLoadFloat4x4(
                                        &stored_instance))));
                        }
                        include_batch_bounds(result, scene, batch);
                        result.batches.push_back(std::move(batch));
                    }
                }
                ++imported_instancers;
            }
            return result;
        }

        CpuObject build_direct_object(
            const Scene& scene,
            ObjectKind kind,
            const std::unordered_map<std::string, std::uint32_t>& node_indices,
            const std::vector<std::uint32_t>& mesh_nodes,
            std::vector<DirectX::XMFLOAT4X4>& world_cache,
            std::vector<std::uint8_t>& world_states) {

            CpuObject result;
            result.kind = kind;
            for (std::uint32_t mesh_index = 0;
                mesh_index < mesh_nodes.size();
                ++mesh_index) {
                const auto node_index = mesh_nodes[mesh_index];
                if (node_index == Scene::INVALID_INDEX) {
                    continue;
                }
                const auto& node = scene.nodes[node_index];
                if (node.object_kind != kind ||
                    (node.flags & Scene::NodeInsideNativePrototype) != 0 ||
                    (node.flags & Scene::NodeVisible) == 0) {
                    continue;
                }
                CpuBatch batch;
                batch.mesh_index = mesh_index;
                batch.transforms.push_back(store_matrix(node_world_matrix(
                    scene,
                    node_index,
                    world_cache,
                    world_states)));
                include_batch_bounds(result, scene, batch);
                result.batches.push_back(std::move(batch));
            }

            if (!result.batches.empty()) {
                return result;
            }

            for (const auto& instance : scene.native_instances) {
                if (instance.object_kind != kind) {
                    continue;
                }
                const auto instance_node = node_indices.find(
                    instance.prim_path);
                const auto prototype_node = node_indices.find(
                    instance.prototype_path);
                if (instance_node == node_indices.end() ||
                    prototype_node == node_indices.end()) {
                    continue;
                }
                const auto instance_world = node_world_matrix(
                    scene,
                    instance_node->second,
                    world_cache,
                    world_states);
                const auto meshes = prototype_meshes(
                    scene,
                    prototype_node->second,
                    mesh_nodes,
                    node_indices);
                for (const auto& prototype_mesh : meshes) {
                    CpuBatch batch;
                    batch.mesh_index = prototype_mesh.mesh_index;
                    batch.transforms.push_back(store_matrix(
                        DirectX::XMMatrixMultiply(
                            DirectX::XMLoadFloat4x4(
                                &prototype_mesh.transform),
                            instance_world)));
                    include_batch_bounds(result, scene, batch);
                    result.batches.push_back(std::move(batch));
                }
                break;
            }
            return result;
        }

        DirectX::XMMATRIX gallery_transform(
            const Bounds& bounds,
            std::uint32_t gallery_index,
            std::uint32_t gallery_count) {

            const float size_x = bounds.maximum.x - bounds.minimum.x;
            const float size_y = bounds.maximum.y - bounds.minimum.y;
            const float size_z = bounds.maximum.z - bounds.minimum.z;
            const float largest_size = std::max({size_x, size_y, size_z});
            if (!bounds.valid || largest_size <= 1.0e-8f) {
                throw std::runtime_error("Selected Jungle object is empty.");
            }
            const DirectX::XMFLOAT3 center{
                (bounds.minimum.x + bounds.maximum.x) * 0.5f,
                (bounds.minimum.y + bounds.maximum.y) * 0.5f,
                (bounds.minimum.z + bounds.maximum.z) * 0.5f
            };
            const auto rows = (gallery_count + GALLERY_COLUMNS - 1) /
                GALLERY_COLUMNS;
            const auto column = gallery_index % GALLERY_COLUMNS;
            const auto row = gallery_index / GALLERY_COLUMNS;
            const float gallery_x =
                (static_cast<float>(column) -
                 (static_cast<float>(GALLERY_COLUMNS) - 1.0f) * 0.5f) *
                GALLERY_CELL_SIZE;
            const float gallery_z =
                ((static_cast<float>(rows) - 1.0f) * 0.5f -
                 static_cast<float>(row)) *
                GALLERY_CELL_SIZE;
            auto result = DirectX::XMMatrixTranslation(
                -center.x,
                -center.y,
                -center.z);
            result = DirectX::XMMatrixMultiply(
                result,
                DirectX::XMMatrixScaling(
                    GALLERY_OBJECT_SIZE / largest_size,
                    GALLERY_OBJECT_SIZE / largest_size,
                    GALLERY_OBJECT_SIZE / largest_size));
            return DirectX::XMMatrixMultiply(
                result,
                DirectX::XMMatrixTranslation(
                    gallery_x,
                    0.0f,
                    gallery_z));
        }

    } // namespace

    struct Renderer::DrawBatch {
        dx::Buffer vertex_buffer;
        dx::Buffer index_buffer;
        dx::Buffer instance_buffer;
        D3D12_VERTEX_BUFFER_VIEW vertex_view{};
        D3D12_INDEX_BUFFER_VIEW index_view{};
        MaterialDescription material;
        std::uint32_t material_index = 0;
        std::uint32_t index_count = 0;
        std::uint32_t instance_count = 0;
    };


    void Renderer::close() {
        if (command_queue_) {
            command_queue_.flush();
            command_context_.set_fence_value(0);
        }
        draw_batches_.clear();
        texture_loader_.reset();
    }

    void Renderer::init(
        void* native_window,
        std::uint32_t width,
        std::uint32_t height,
        const cooker::JungleScene& scene) {

        if (native_window == nullptr || width == 0 || height == 0) {
            throw std::invalid_argument("Invalid renderer window size.");
        }

        HWND window = static_cast<HWND>(native_window);

        factory_ = dx::DeviceUtils::create_factory();
        device_ = dx::DeviceUtils::create_device(factory_.Get());
        command_queue_.init(
            device_.Get(),
            D3D12_COMMAND_LIST_TYPE_DIRECT);
        swap_chain_.init(
            factory_.Get(),
            command_queue_.get_command_queue(),
            window,
            width,
            height,
            FRAME_COUNT,
            true);

        rtv_heap_.init(
            device_.Get(),
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            FRAME_COUNT,
            false);
        dsv_heap_.init(
            device_.Get(),
            D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            1,
            false);
        for (UINT i = 0; i < FRAME_COUNT; ++i) {
            Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
            throw_if_failed(swap_chain_->GetBuffer(
                i,
                IID_PPV_ARGS(buffer.ReleaseAndGetAddressOf())),
                "IDXGISwapChain::GetBuffer failed.");
            device_->CreateRenderTargetView(
                buffer.Get(),
                nullptr,
                rtv_heap_.get_cpu_handle(i));
        }
        init_depth_buffer(width, height);

        command_context_.init(
            device_.Get(),
            D3D12_COMMAND_LIST_TYPE_DIRECT);

        enum class RootParameter : UINT {
            ViewProjection,
            InstanceTransforms,
            MaterialConstants,
            MaterialTextures,
            MaterialSampler,
            Count
        };
        dx::RootSignatureBuilder root_builder;
        root_builder.init(RootParameter::Count);
        root_builder.set_constants(RootParameter::ViewProjection)
            .reg(0)
            .count(16)
            .vis_vertex()
            .add();
        root_builder.set_root_srv(RootParameter::InstanceTransforms)
            .reg(0)
            .vis_vertex()
            .add();
        root_builder.set_constants(RootParameter::MaterialConstants)
            .reg(1)
            .count(16)
            .vis_pixel()
            .add();
        root_builder.set_resource_table(RootParameter::MaterialTextures)
            .srv()
            .reg(1)
            .count(4)
            .add_range()
            .vis_pixel()
            .add();
        root_builder.set_sampler_table(RootParameter::MaterialSampler)
            .sampler()
            .reg(0)
            .count(4)
            .add_range()
            .vis_pixel()
            .add();
        root_builder.set_flags(
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
        root_signature_ = root_builder.build(device_.Get());

        dx::Shader vertex_shader;
        vertex_shader.load(
            std::filesystem::path{FASTJUNGLE_SHADER_OUTPUT_DIR} /
            "JungleScene.vs.dxil");

        dx::Shader pixel_shader;
        pixel_shader.load(
            std::filesystem::path{FASTJUNGLE_SHADER_OUTPUT_DIR} /
            "JungleScene.ps.dxil");

        constexpr D3D12_INPUT_ELEMENT_DESC input_elements[]{
            {
                "POSITION",
                0,
                DXGI_FORMAT_R32G32B32_FLOAT,
                0,
                0,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },
            {
                "NORMAL",
                0,
                DXGI_FORMAT_R32G32B32_FLOAT,
                0,
                12,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },
            {
                "TANGENT",
                0,
                DXGI_FORMAT_R32G32B32A32_FLOAT,
                0,
                24,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },
            {
                "TEXCOORD",
                0,
                DXGI_FORMAT_R32G32_FLOAT,
                0,
                40,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            }
        };
        auto description = dx::PSOUtils::default_graphics_desc();
        description.pRootSignature = root_signature_.Get();
        description.VS = vertex_shader.get_bytecode();
        description.PS = pixel_shader.get_bytecode();
        description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        description.DepthStencilState.DepthEnable = TRUE;
        description.DepthStencilState.DepthWriteMask =
            D3D12_DEPTH_WRITE_MASK_ALL;
        description.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        description.InputLayout = {
            input_elements,
            static_cast<UINT>(std::size(input_elements))
        };
        description.NumRenderTargets = 1;
        description.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        pipeline_state_ = dx::PSOUtils::create_graphics(
            device_.Get(),
            description);

        command_context_.reset();
        build_scene_geometry(scene);
        command_context_.close();

        command_queue_.execute(command_context_.get_command_list());
        command_queue_.flush();
        command_context_.set_fence_value(0);

        texture_loader_->finish_uploads();
        update_camera(width, height);
    }

    void Renderer::init_depth_buffer(uint32_t width, uint32_t height) {
        D3D12_RESOURCE_DESC description{};
        description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        description.Width = width;
        description.Height = height;
        description.DepthOrArraySize = 1;
        description.MipLevels = 1;
        description.Format = DXGI_FORMAT_D32_FLOAT;
        description.SampleDesc.Count = 1;
        description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        description.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clear_value{};
        clear_value.Format = DXGI_FORMAT_D32_FLOAT;
        clear_value.DepthStencil.Depth = 1.0f;
        clear_value.DepthStencil.Stencil = 0;
        depth_buffer_ = {};
        depth_buffer_.init(
            device_.Get(),
            description,
            dx::TextureType::texture2d,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clear_value);
        D3D12_DEPTH_STENCIL_VIEW_DESC view_description{};
        view_description.Format = DXGI_FORMAT_D32_FLOAT;
        view_description.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        view_description.Flags = D3D12_DSV_FLAG_NONE;
        view_description.Texture2D.MipSlice = 0;
        device_->CreateDepthStencilView(
            depth_buffer_.get(),
            &view_description,
            dsv_heap_.get_cpu_handle(0));
    }

    void Renderer::build_scene_geometry(
        const cooker::JungleScene& scene) {

        std::unordered_map<std::string, std::uint32_t> node_indices;
        node_indices.reserve(scene.nodes.size());
        for (std::uint32_t index = 0; index < scene.nodes.size(); ++index) {
            node_indices.emplace(scene.nodes[index].path, index);
        }
        const auto mesh_nodes = build_mesh_node_map(scene);
        std::vector<DirectX::XMFLOAT4X4> world_cache(scene.nodes.size());
        std::vector<std::uint8_t> world_states(scene.nodes.size(), 0);

        std::vector<CpuObject> objects;
        for (const auto kind : RENDERABLE_OBJECT_KINDS) {
            if (!should_render_kind(kind)) {
                continue;
            }
            auto object = is_point_instanced_kind(kind)
                ? build_point_object(
                    scene,
                    kind,
                    node_indices,
                    mesh_nodes,
                    world_cache,
                    world_states)
                : build_direct_object(
                    scene,
                    kind,
                    node_indices,
                    mesh_nodes,
                    world_cache,
                    world_states);
            if (object.batches.empty() || !object.bounds.valid) {
                throw std::runtime_error(
                    std::string{"No renderable geometry for Jungle object "} +
                    Scene::object_kind_name(kind));
            }
            objects.push_back(std::move(object));
        }

        if (objects.empty()) {
            throw std::runtime_error("No Jungle object kind was selected.");
        }

        MaterialResolver material_resolver{scene};
        std::unordered_set<std::string> resolved_materials;
        Bounds gallery_bounds;
        for (std::uint32_t object_index = 0;
            object_index < objects.size();
            ++object_index) {
            auto& object = objects[object_index];
            const auto placement = gallery_transform(
                object.bounds,
                object_index,
                static_cast<std::uint32_t>(objects.size()));

            for (auto& cpu_batch : object.batches) {
                const auto& mesh = scene.meshes[cpu_batch.mesh_index];
                if (mesh.points.empty() || cpu_batch.transforms.empty()) {
                    continue;
                }

                std::vector<DirectX::XMFLOAT4X4> placed_transforms;
                std::vector<GpuInstance> gpu_instances;
                placed_transforms.reserve(cpu_batch.transforms.size());
                gpu_instances.reserve(cpu_batch.transforms.size());
                for (const auto& stored_transform : cpu_batch.transforms) {
                    const auto transform = DirectX::XMMatrixMultiply(
                        DirectX::XMLoadFloat4x4(&stored_transform),
                        placement);
                    placed_transforms.push_back(store_matrix(transform));
                    gpu_instances.push_back(make_gpu_instance(transform));
                }

                for (const auto& stored_transform : placed_transforms) {
                    const auto transform = DirectX::XMLoadFloat4x4(
                        &stored_transform);
                    for (const auto& point : mesh.points) {
                        gallery_bounds.include(
                            DirectX::XMVector3TransformCoord(
                                DirectX::XMVectorSet(
                                    point.x,
                                    point.y,
                                    point.z,
                                    1.0f),
                                transform));
                        }
                }

                for (const auto& face_group : build_face_groups(
                    scene,
                    mesh)) {
                    auto material = material_resolver.resolve(
                        face_group.material_path);
                    auto geometry = build_geometry_part(
                        mesh,
                        face_group,
                        material.uv_primvar);
                    if (geometry.indices.empty()) {
                        continue;
                    }
                    if (geometry.vertices.size() * sizeof(Vertex) >
                            std::numeric_limits<UINT>::max() ||
                        geometry.indices.size() * sizeof(std::uint32_t) >
                            std::numeric_limits<UINT>::max()) {
                        throw std::runtime_error(
                            "Jungle mesh exceeds a D3D12 buffer view.");
                    }

                    resolved_materials.insert(material.material_path);
                    auto batch = std::make_unique<DrawBatch>();
                    upload_vector(
                        batch->vertex_buffer,
                        device_.Get(),
                        geometry.vertices);
                    upload_vector(
                        batch->index_buffer,
                        device_.Get(),
                        geometry.indices);
                    upload_vector(
                        batch->instance_buffer,
                        device_.Get(),
                        gpu_instances);
                    batch->vertex_view.BufferLocation =
                        batch->vertex_buffer->GetGPUVirtualAddress();
                    batch->vertex_view.SizeInBytes = static_cast<UINT>(
                        geometry.vertices.size() * sizeof(Vertex));
                    batch->vertex_view.StrideInBytes = sizeof(Vertex);
                    batch->index_view.BufferLocation =
                        batch->index_buffer->GetGPUVirtualAddress();
                    batch->index_view.SizeInBytes = static_cast<UINT>(
                        geometry.indices.size() * sizeof(std::uint32_t));
                    batch->index_view.Format = DXGI_FORMAT_R32_UINT;
                    batch->material = std::move(material);
                    batch->index_count = static_cast<std::uint32_t>(
                        geometry.indices.size());
                    batch->instance_count = static_cast<std::uint32_t>(
                        gpu_instances.size());
                    draw_batches_.push_back(std::move(batch));
                }
            }
        }

        if (!gallery_bounds.valid || draw_batches_.empty()) {
            throw std::runtime_error("Jungle gallery contains no triangles.");
        }
        texture_loader_ = std::make_unique<TextureLoader>();
        texture_loader_->init(
            device_.Get(),
            command_context_.get_command_list(),
            draw_batches_.size());
        for (auto& batch : draw_batches_) {
            batch->material_index = texture_loader_->add_material(
                batch->material);
        }
        uint32_t resolved_material_count = static_cast<std::uint32_t>(
            resolved_materials.size());
        (void)resolved_material_count;
        render_bounds_ = {
            gallery_bounds.minimum.x,
            gallery_bounds.minimum.y,
            gallery_bounds.minimum.z,
            gallery_bounds.maximum.x,
            gallery_bounds.maximum.y,
            gallery_bounds.maximum.z
        };
        uint32_t  rendered_kind_count = static_cast<std::uint32_t>(objects.size());
        if (RENDER_ALL_OBJECT_KINDS &&
            rendered_kind_count != RENDERABLE_OBJECT_KINDS.size()) {
            throw std::runtime_error(
                "Not all renderable Jungle object kinds were imported.");
        }
    }

    void Renderer::update_camera(uint32_t width, uint32_t height) {
        const DirectX::XMFLOAT3 minimum{
            render_bounds_[0],
            render_bounds_[1],
            render_bounds_[2]
        };
        const DirectX::XMFLOAT3 maximum{
            render_bounds_[3],
            render_bounds_[4],
            render_bounds_[5]
        };
        const DirectX::XMFLOAT3 center{
            (minimum.x + maximum.x) * 0.5f,
            (minimum.y + maximum.y) * 0.5f,
            (minimum.z + maximum.z) * 0.5f
        };
        const auto target = DirectX::XMLoadFloat3(&center);
        const auto direction = DirectX::XMVector3Normalize(
            DirectX::XMVectorSet(0.0f, 1.0f, -0.65f, 0.0f));
        const auto diagonal = DirectX::XMVectorSet(
            maximum.x - minimum.x,
            maximum.y - minimum.y,
            maximum.z - minimum.z,
            0.0f);
        const float radius = DirectX::XMVectorGetX(
            DirectX::XMVector3Length(diagonal)) * 0.5f;
        const float camera_distance = radius * 2.0f + 10.0f;
        const auto eye = DirectX::XMVectorSubtract(
            target,
            DirectX::XMVectorScale(direction, camera_distance));
        const auto view = DirectX::XMMatrixLookAtLH(
            eye,
            target,
            DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));

        Bounds view_bounds;
        for (const float x : {minimum.x, maximum.x}) {
            for (const float y : {minimum.y, maximum.y}) {
                for (const float z : {minimum.z, maximum.z}) {
                    view_bounds.include(DirectX::XMVector3TransformCoord(
                        DirectX::XMVectorSet(x, y, z, 1.0f),
                        view));
                }
            }
        }

        const float aspect = static_cast<float>(width) /
            static_cast<float>(height);
        float view_width = view_bounds.maximum.x -
            view_bounds.minimum.x + 1.0f;
        float view_height = view_bounds.maximum.y -
            view_bounds.minimum.y + 1.0f;
        if (view_width / view_height < aspect) {
            view_width = view_height * aspect;
        }
        else {
            view_height = view_width / aspect;
        }
        const float depth_padding = std::max(
            1.0f,
            (view_bounds.maximum.z - view_bounds.minimum.z) * 0.1f);
        const float near_plane = std::max(
            0.1f,
            view_bounds.minimum.z - depth_padding);
        const float far_plane = std::max(
            near_plane + 1.0f,
            view_bounds.maximum.z + depth_padding);
        const auto projection = DirectX::XMMatrixOrthographicLH(
            view_width,
            view_height,
            near_plane,
            far_plane);
        const auto stored = store_matrix(DirectX::XMMatrixMultiply(
            view,
            projection));
        std::memcpy(
            view_projection_.data(),
            &stored,
            sizeof(stored));
    }

    void Renderer::resize(
        std::uint32_t width,
        std::uint32_t height) {

        command_queue_.flush();
        command_context_.set_fence_value(0);
        depth_buffer_ = {};

        swap_chain_.resize(width, height);

        for (UINT i = 0; i < FRAME_COUNT; ++i) {
            Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
            throw_if_failed(swap_chain_->GetBuffer(
                i,
                IID_PPV_ARGS(buffer.ReleaseAndGetAddressOf())),
                "IDXGISwapChain::GetBuffer failed after resize.");
            device_->CreateRenderTargetView(
                buffer.Get(),
                nullptr,
                rtv_heap_.get_cpu_handle(i));
        }
        init_depth_buffer(width, height);
        update_camera(width, height);
    }

    void Renderer::render() {
        command_queue_.wait(command_context_.get_fence_value());
        command_context_.reset(pipeline_state_.Get());

        swap_chain_.get_current_buffer().transition(
            command_context_.get_command_list(), D3D12_RESOURCE_STATE_RENDER_TARGET);

        const UINT frame_index = swap_chain_.get_current_frame();
        const auto rtv = rtv_heap_.get_cpu_handle(frame_index);
        const auto dsv = dsv_heap_.get_cpu_handle(0);
        constexpr float clear_color[] = {0.06f, 0.075f, 0.10f, 1.0f};
        command_context_->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        command_context_->ClearRenderTargetView(
            rtv,
            clear_color,
            0,
            nullptr);
        command_context_->ClearDepthStencilView(
            dsv,
            D3D12_CLEAR_FLAG_DEPTH,
            1.0f,
            0,
            0,
            nullptr);
        
        const UINT width = swap_chain_.get_width();
        const UINT height = swap_chain_.get_height();
        command_context_.RSSetViewPortScissorRect(width, height);
        command_context_.SetDescriptorHeaps(
            texture_loader_->resource_heap(), texture_loader_->sampler_heap());
        command_context_->SetGraphicsRootSignature(root_signature_.Get());
        command_context_->SetGraphicsRoot32BitConstants(
            0,
            static_cast<UINT>(view_projection_.size()),
            view_projection_.data(),
            0);
        command_context_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        for (const auto& batch : draw_batches_) {
            command_context_->IASetVertexBuffers(
                0,
                1,
                &batch->vertex_view);
            command_context_->IASetIndexBuffer(&batch->index_view);
            command_context_->SetGraphicsRootShaderResourceView(
                1,
                batch->instance_buffer->GetGPUVirtualAddress());
            command_context_->SetGraphicsRoot32BitConstants(
                2,
                16,
                &batch->material.constants,
                0);
            command_context_->SetGraphicsRootDescriptorTable(
                3,
                texture_loader_->material_handle(batch->material_index));
            command_context_->SetGraphicsRootDescriptorTable(
                4,
                texture_loader_->sampler_handle(batch->material_index));
            command_context_->DrawIndexedInstanced(
                batch->index_count,
                batch->instance_count,
                0,
                0,
                0);
        }

        swap_chain_.get_current_buffer().transition(
            command_context_.get(), D3D12_RESOURCE_STATE_PRESENT);
        command_context_.close();

        command_queue_.execute(command_context_.get());
        swap_chain_.present();

        command_context_.set_fence_value(command_queue_.signal());
    }

} // namespace fjr
