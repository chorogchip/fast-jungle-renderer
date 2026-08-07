#include "FastJungle/renderer/data/DataPerFrame.hpp"

#include "FastJungle/renderer/Camera.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render::data {

    void DataPerFrame::CameraConstants::fill_from_camera(
        const Camera& camera) {

        view_projection = camera.get_view_projection_mat();
        world_position = camera.get_position();

        const auto vp = XMLoadFloat4x4(&view_projection);

        const auto colx = DirectX::XMVectorSet(
            view_projection._11,
            view_projection._21,
            view_projection._31,
            view_projection._41);

        const auto coly = DirectX::XMVectorSet(
            view_projection._12,
            view_projection._22,
            view_projection._32,
            view_projection._42);

        const auto colz = DirectX::XMVectorSet(
            view_projection._13,
            view_projection._23,
            view_projection._33,
            view_projection._43);

        const auto colw = DirectX::XMVectorSet(
            view_projection._14,
            view_projection._24,
            view_projection._34,
            view_projection._44);

        DirectX::XMVECTOR planes[6];

        planes[0] = DirectX::XMVectorAdd(colw, colx);
        planes[1] = DirectX::XMVectorSubtract(colw, colx);

        planes[2] = DirectX::XMVectorAdd(colw, coly);
        planes[3] = DirectX::XMVectorSubtract(colw, coly);

        planes[4] = colz;
        planes[5] = DirectX::XMVectorSubtract(colw, colz);

        for (int i = 0; i < 6; ++i) {
            planes[i] = DirectX::XMPlaneNormalize(planes[i]);
            DirectX::XMStoreFloat4(
                &normalized_frustum_planes[i], planes[i]);
        }

        // buf.environment_world_transform = environment.world_transform;
        // buf.environment_color = environment.color;
        // buf.environment_intensity = environment.intensity * std::exp2(environment.exposure);
        // buf.environment_texture_id = environment.texture;

        // TODO
        lod_projection_scale = 0.0f;
        lod_pixel_threshold = 1.0f;
        spatial_cluster_count = 88888888;
        mesh_lod_count = 12312;
    }

    DataPerFrame DataPerFrame::build(
        ID3D12Device* device,
        uint32_t instance_count,
        uint32_t spacial_cluster_count) {

        DataPerFrame ret{};
        ret.camera.init(device);

        // spacial_cluster_count = scene.mesh_lods.size();

        constexpr uint32_t MAX_INDIRECT_DRAW_COUNT_PER_CLASS =
            256u * data::Consts::LOD_CNT;

        const UINT64 indirect_capacity =
            static_cast<UINT64>(MAX_INDIRECT_DRAW_COUNT_PER_CLASS) *
            data::Consts::RASTER_CLASS_CNT;


        ret.indirect_gpu_draw.init(
            device,
            indirect_capacity * sizeof(IndirectGPUDraw),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COMMON);

        ret.indirect_gpu_draw_counts.init(
            device,
            data::Consts::RASTER_CLASS_CNT * sizeof(std::uint32_t),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COMMON);

        ret.visible_instance.init(
            device,
            instance_count * sizeof(std::uint32_t),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COMMON);


        const UINT64 bin_byte_size =
            static_cast<UINT64>(spacial_cluster_count) *
            sizeof(std::uint32_t);

        ret.bin_counts.init(
            device,
            bin_byte_size,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COMMON);

        ret.bin_offsets.init(
            device,
            bin_byte_size,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COMMON);

        ret.bin_cursors.init(
            device,
            bin_byte_size,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COMMON);

        return ret;
    }

}  // namespace fjr::render::data