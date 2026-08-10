#include "FastJungle/renderer/data/geometry/BuilderGeomVertex.hpp"

namespace fjr::render::data::geom {

    namespace {

        uint32_t normal_to_10u(float value) {
            const float normalized = std::clamp(
                value * 0.5f + 0.5f, 0.0f, 1.0f);
            return static_cast<uint32_t>(
                std::lround(normalized * 1023.0f));
        }

        uint16_t quantize_unorm16(
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

            return static_cast<uint16_t>(std::min(q, 65535u));
        }

        DataPersistent::PackedPosition pack_position(
            const DirectX::XMFLOAT3& value,
            const DirectX::XMFLOAT3& minimum,
            const DirectX::XMFLOAT3& extent) {

            DataPersistent::PackedPosition ret{};

            ret.x = quantize_unorm16(value.x, minimum.x, extent.x);
            ret.y = quantize_unorm16(value.y, minimum.y, extent.y);
            ret.z = quantize_unorm16(value.z, minimum.z, extent.z);

            return ret;
        }

        DataPersistent::PackedNormal pack_normal(
            const DirectX::XMFLOAT3& value) {

            DataPersistent::PackedNormal ret{};

            ret.value =
                normal_to_10u(value.x) |
                (normal_to_10u(value.y) << 10u) |
                (normal_to_10u(value.z) << 20u) |
                (3u << 30u);

            return ret;
        }

        DataPersistent::PackedUV pack_uv(
            const DirectX::XMFLOAT2& value,
            const DirectX::XMFLOAT3& minimum,
            const DirectX::XMFLOAT3& extent) {

            DataPersistent::PackedUV ret{};

            ret.x = quantize_unorm16(value.x, minimum.x, extent.x);
            ret.y = quantize_unorm16(value.y, minimum.y, extent.y);

            return ret;
        }

    }

    std::vector<DataPersistent::VertexDecodeParams> BuilderGeomVertex::build(
        std::vector<DataPersistent::PackedPosition>& dest_pos,
        std::vector<DataPersistent::PackedNormal>& dest_normal,
        std::vector<DataPersistent::PackedUV>& dest_uv,
        const scene::StaticScene& scene) {

        std::vector<DataPersistent::VertexDecodeParams> ret{};
        ret.resize(scene.submeshes.size());

        dest_pos.resize(scene.vertices.size());
        dest_normal.resize(scene.vertices.size());
        dest_uv.resize(scene.vertices.size());

        for (uint32_t index = 0; index < scene.vertices.size(); ++index)
            dest_normal[index] = pack_normal(scene.vertices[index].normal);

        for (uint32_t index = 0; index < scene.submeshes.size(); ++index) {

            const auto& submesh = scene.submeshes[index];
            auto& decode = ret[index];

            math::AABB aabb_pos{};
            math::AABB aabb_uv{};

            for (uint32_t vertex = 0; vertex < submesh.vertex_count; ++vertex) {

                const auto& source =
                    scene.vertices[submesh.vertex_offset + vertex];

                aabb_pos.merge(source.position);
                aabb_uv.merge(source.uv.x, source.uv.y, 0.0f);
            }

            const DirectX::XMFLOAT3 pos_min = aabb_pos.min;
            const DirectX::XMFLOAT3 pos_extent = aabb_pos.get_size();
            const DirectX::XMFLOAT3 uv_min = aabb_uv.min;
            const DirectX::XMFLOAT3 uv_extent = aabb_uv.get_size();

            decode.position_min = {
                pos_min.x, pos_min.y, pos_min.z, 0.0f
            };
            decode.position_extent = {
                pos_extent.x, pos_extent.y, pos_extent.z, 0.0f
            };
            decode.uv_min_extent = {
                uv_min.x, uv_min.y, uv_extent.x, uv_extent.y
            };

            for (uint32_t vertex = 0; vertex < submesh.vertex_count; ++vertex) {

                const uint32_t destination = submesh.vertex_offset + vertex;
                const auto& source = scene.vertices[destination];

                dest_pos[destination] =
                    pack_position(source.position, pos_min, pos_extent);
                dest_uv[destination] =
                    pack_uv(source.uv, uv_min, uv_extent);
            }
        }

        return ret;
    }

}