#pragma once

#include <cstddef>
#include <cstdint>
#include <DirectXMath.h>

#include "FastJungle/dx12/MappedCBuffer.hpp"
#include "FastJungle/renderer/Camera.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render {
    class Camera;
}

namespace fjr::render::data {

    struct DataPerFrame {

        struct alignas(Consts::CBUF_ALIGN) CameraConstants {
            DirectX::XMFLOAT4X4 view_projection = Consts::I_MAT;
            DirectX::XMFLOAT3 world_position{};
            float world_position_padding = 0.0f;
            DirectX::XMFLOAT4 normalized_frustum_planes[6];

            float lod_projection_scale = 0.0f;
            float lod_error_threshold_px = 0.0f;
            float impostor_transition_radius_px = 0.0f;
            float cull_radius_px = 0.0f;

            uint32_t spatial_cluster_count = 0;
            uint32_t mesh_lod_count = 0;
            uint32_t cam_pixel_width = 0;
            uint32_t cam_pixel_height = 0;

            DirectX::XMFLOAT3 environment_color{};
            float environment_intensity = 0.0f;
            uint32_t environment_texture = Consts::IND_ERR;
            DirectX::XMFLOAT3 environment_padding{};

            void fill_from_camera(
                const Camera& camera,
                uint32_t viewport_width,
                uint32_t viewport_height,
                uint32_t scene_spatial_cluster_count,
                uint32_t scene_mesh_lod_count,
                const scene::StaticScene::EnvironmentLight& environment);
        };
        static_assert(sizeof(CameraConstants) == Consts::CBUF_ALIGN);
        static_assert(
            offsetof(CameraConstants, normalized_frustum_planes) == 80);

        dx::MappedCBuffer<CameraConstants> camera;


        struct alignas(Consts::CBUF_ALIGN) CullingConstants {
            DirectX::XMFLOAT4 frustum_planes[6];

            DirectX::XMFLOAT3 camera_position;
            float lod_projection_scale;

            float lod_pixel_threshold;
            uint32_t spatial_cluster_count;
            uint32_t mesh_lod_count;
            uint32_t padding;
        };

        dx::MappedCBuffer<CullingConstants> culling;




        // binned by meshlod. count of this struct == cnt of submesh in visible meshlod.
        struct IndirectGPUDraw {
            uint32_t visible_instance_offset = Consts::IND_ERR;
            uint32_t material_id = Consts::IND_ERR;
            uint32_t submesh_id = Consts::IND_ERR;
            D3D12_DRAW_INDEXED_ARGUMENTS draw_arguments{};

            static constexpr inline uint32_t ROOT_CONST_CNT = 3;
        };
        static_assert(sizeof(IndirectGPUDraw) == 32);
        static_assert(offsetof(IndirectGPUDraw, draw_arguments) == 12);
        static_assert(std::is_trivially_copyable_v<IndirectGPUDraw>);

        dx::Buffer indirect_gpu_draw{};
        dx::Buffer indirect_gpu_draw_counts{};
        dx::Buffer visible_instance{};  // first uint32_t, instance transform id. later uint16_t.

        static DataPerFrame build(
            ID3D12Device* device,
            uint32_t instance_count,
            uint32_t indirect_draw_capacity_per_class);
    };

}  // namespace fjr::render::data
