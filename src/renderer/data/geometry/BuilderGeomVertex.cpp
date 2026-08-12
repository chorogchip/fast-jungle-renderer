#include "FastJungle/renderer/data/geometry/BuilderGeomVertex.hpp"

#include <algorithm>
#include <cmath>
#include <span>

#include "FastJungle/core/math/AABB.hpp"
#include "FastJungle/core/util/EnumUtils.hpp"

namespace fjr::render::data::geom {

    namespace {

        struct PackingParams {
            DirectX::XMFLOAT3 position_min;
            DirectX::XMFLOAT3 position_extent;
            DirectX::XMFLOAT2 uv_min;
            DirectX::XMFLOAT2 uv_extent;
        };

        struct PackedVertex {
            DataPersistent::PackedPosition position;
            DataPersistent::PackedNormal normal;
            DataPersistent::PackedUV uv;
        };

        uint32_t normal_to_10u(float value) {
            const float normalized = std::clamp(
                value * 0.5f + 0.5f, 0.0f, 1.0f);
            return static_cast<uint32_t>(
                std::lround(normalized * 1023.0f));
        }

        std::uint16_t quantize_unorm16(
            float value,
            float minimum,
            float extent) {

            if (extent == 0.0f)
                return 0;

            const double normalized =
                (static_cast<double>(value) -
                    static_cast<double>(minimum)) /
                static_cast<double>(extent);

            const double clamped = std::clamp(normalized, 0.0, 1.0);
            const auto q = static_cast<uint32_t>(
                std::floor(clamped * 65535.0 + 0.5));

            return static_cast<std::uint16_t>(std::min(q, 65535u));
        }

        DataPersistent::PackedPosition pack_position(
            const DirectX::XMFLOAT3& value,
            const DirectX::XMFLOAT3& minimum,
            const DirectX::XMFLOAT3& extent) {

            DataPersistent::PackedPosition result{};

            result.x = quantize_unorm16(value.x, minimum.x, extent.x);
            result.y = quantize_unorm16(value.y, minimum.y, extent.y);
            result.z = quantize_unorm16(value.z, minimum.z, extent.z);

            return result;
        }

        DataPersistent::PackedNormal pack_normal(
            const DirectX::XMFLOAT3& value) {

            DataPersistent::PackedNormal result{};

            result.value =
                normal_to_10u(value.x) |
                (normal_to_10u(value.y) << 10u) |
                (normal_to_10u(value.z) << 20u) |
                (3u << 30u);

            return result;
        }

        DataPersistent::PackedUV pack_uv(
            const DirectX::XMFLOAT2& value,
            const DirectX::XMFLOAT2& minimum,
            const DirectX::XMFLOAT2& extent) {

            DataPersistent::PackedUV result{};

            result.x = quantize_unorm16(value.x, minimum.x, extent.x);
            result.y = quantize_unorm16(value.y, minimum.y, extent.y);

            return result;
        }

        PackingParams build_packing_params(
            std::span<const scene::StaticScene::Vertex> vertices) {

            math::AABB position_bounds{};
            math::AABB uv_bounds{};

            for (const auto& vertex : vertices) {
                position_bounds.merge(vertex.position);
                uv_bounds.merge(vertex.uv.x, vertex.uv.y, 0.0f);
            }

            const auto uv_extent = uv_bounds.get_size();

            return {
                .position_min = position_bounds.min,
                .position_extent = position_bounds.get_size(),
                .uv_min = { uv_bounds.min.x, uv_bounds.min.y },
                .uv_extent = { uv_extent.x, uv_extent.y },
            };
        }

        DataPersistent::VertexDecodeParams build_decode_params(
            const PackingParams& params) {

            DataPersistent::VertexDecodeParams result{};
            result.position_min = {
                params.position_min.x,
                params.position_min.y,
                params.position_min.z,
                0.0f,
            };
            result.position_extent = {
                params.position_extent.x,
                params.position_extent.y,
                params.position_extent.z,
                0.0f,
            };
            result.uv_min_extent = {
                params.uv_min.x,
                params.uv_min.y,
                params.uv_extent.x,
                params.uv_extent.y,
            };

            return result;
        }

        PackedVertex pack_vertex(
            const scene::StaticScene::Vertex& source,
            const PackingParams& params) {

            return {
                .position = pack_position(
                    source.position,
                    params.position_min,
                    params.position_extent),
                .normal = pack_normal(source.normal),
                .uv = pack_uv(
                    source.uv,
                    params.uv_min,
                    params.uv_extent),
            };
        }

        std::int32_t append_alpha_vertices(
            BuilderGeomVertex::Result& result,
            std::span<const scene::StaticScene::Vertex> vertices,
            const PackingParams& params) {

            const auto base_vertex = static_cast<std::int32_t>(
                result.alpha_visibility.size());

            for (const auto& source : vertices) {
                const auto packed = pack_vertex(source, params);
                result.alpha_visibility.push_back({
                    packed.position,
                    packed.uv,
                });
                result.alpha_shading.push_back({ packed.normal });
            }

            return base_vertex;
        }

        std::int32_t append_opaque_vertices(
            BuilderGeomVertex::Result& result,
            std::span<const scene::StaticScene::Vertex> vertices,
            const PackingParams& params) {

            const auto base_vertex = static_cast<std::int32_t>(
                result.opaque_visibility.size());

            for (const auto& source : vertices) {
                const auto packed = pack_vertex(source, params);
                result.opaque_visibility.push_back({ packed.position });
                result.opaque_shading.push_back({
                    packed.normal,
                    packed.uv,
                });
            }

            return base_vertex;
        }

        void reserve_vertex_streams(
            BuilderGeomVertex::Result& result,
            const scene::StaticScene& scene) {

            std::size_t opaque_vertex_count = 0;
            std::size_t alpha_vertex_count = 0;

            for (const auto& submesh : scene.submeshes) {
                const bool alpha_tested = enm::has(
                    submesh.flags,
                    scene::StaticScene::EnumSubmeshFlag::ALPHA_TESTED);

                auto& vertex_count = alpha_tested
                    ? alpha_vertex_count
                    : opaque_vertex_count;
                vertex_count += submesh.vertex_count;
            }

            result.opaque_visibility.reserve(opaque_vertex_count);
            result.opaque_shading.reserve(opaque_vertex_count);
            result.alpha_visibility.reserve(alpha_vertex_count);
            result.alpha_shading.reserve(alpha_vertex_count);
        }

    } // namespace

    BuilderGeomVertex::Result BuilderGeomVertex::build(
        const scene::StaticScene& scene) {

        Result result{};
        result.decode_params.resize(scene.submeshes.size());
        result.submesh_base_vertices.resize(scene.submeshes.size());
        reserve_vertex_streams(result, scene);

        const std::span<const scene::StaticScene::Vertex> scene_vertices{
            scene.vertices
        };

        for (std::size_t index = 0; index < scene.submeshes.size(); ++index) {
            const auto& submesh = scene.submeshes[index];
            const auto vertices = scene_vertices.subspan(
                submesh.vertex_offset,
                submesh.vertex_count);
            const auto packing_params = build_packing_params(vertices);

            result.decode_params[index] =
                build_decode_params(packing_params);

            const bool alpha_tested = enm::has(
                submesh.flags,
                scene::StaticScene::EnumSubmeshFlag::ALPHA_TESTED);

            result.submesh_base_vertices[index] = alpha_tested
                ? append_alpha_vertices(result, vertices, packing_params)
                : append_opaque_vertices(result, vertices, packing_params);
        }

        return result;
    }

}
