#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>
#include <d3d12.h>

#include "FastJungle/dx12/ResourceUploader.hpp"
#include "FastJungle/dx12/Buffer.hpp"
#include "FastJungle/dx12/Texture.hpp"
#include "FastJungle/dx12/View.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render::data {

    struct DataPersistent {

        struct Material {
            DirectX::XMFLOAT3 base_color{ 1.0f, 1.0f, 1.0f };
            float roughness = 0.5f;

            uint32_t texture_basecolor = Consts::IND_ERR;
            uint32_t texture_normal = Consts::IND_ERR;
            uint32_t texture_roughness = Consts::IND_ERR;
            uint32_t texture_opacity = Consts::IND_ERR;

            uint32_t flags = 0;
            DirectX::XMFLOAT3 impostor_center{};
            float impostor_half_width = 0.0f;
            float impostor_half_height = 0.0f;
            DirectX::XMFLOAT2 padding{};
        };
        static_assert(sizeof(Material) == 64);
        static_assert(std::is_trivially_copyable_v<Material>);

        dx::Buffer material{};
        std::vector<dx::Texture> textures;
        dx::DescAlloc texture_descriptors{};
        dx::DescAlloc samplers{};  // only 2 samplers
        uint32_t wrap_sampler = 0;
        uint32_t clamp_sampler = 0;

        struct PackedPosition {
            uint16_t x = 0;
            uint16_t y = 0;
            uint16_t z = 0;
            uint16_t w = 0;
        };
        static_assert(sizeof(PackedPosition) == 8);
        static_assert(std::is_trivially_copyable_v<PackedPosition>);

        struct PackedNormal {
            uint32_t value = 0;
        };
        static_assert(sizeof(PackedNormal) == 4);
        static_assert(std::is_trivially_copyable_v<PackedNormal>);

        struct PackedUV {
            uint16_t x = 0;
            uint16_t y = 0;
        };
        static_assert(sizeof(PackedUV) == 4);
        static_assert(std::is_trivially_copyable_v<PackedUV>);

        dx::Buffer vertex_pos{};
        dx::Buffer vertex_normal{};
        dx::Buffer vertex_uv{};
        dx::Buffer index{};

        struct VertexDecodeParams {
            DirectX::XMFLOAT4 position_min{};
            DirectX::XMFLOAT4 position_extent{};
            DirectX::XMFLOAT4 uv_min_extent{};
        };
        static_assert(sizeof(VertexDecodeParams) == 48);
        static_assert(std::is_trivially_copyable_v<VertexDecodeParams>);

        dx::Buffer vertex_decode_params{};


        struct InstanceTransform {
            DirectX::XMFLOAT3 position{};
            DirectX::XMFLOAT4 rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
            DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };
        };
        static_assert(sizeof(InstanceTransform) == 40);
        static_assert(std::is_trivially_copyable_v<InstanceTransform>);

        dx::Buffer instance_transform{};


        struct SubMesh {
            uint32_t material_id = Consts::IND_ERR;
            EnumRasterClass raster_class =
                EnumRasterClass::OPAQUE_SINGLE_SIDED;
            uint32_t index_offset = Consts::IND_ERR;
            uint32_t index_count = 0;
            int32_t base_vertex = 0;
        };
        static_assert(sizeof(SubMesh) == 20);
        static_assert(std::is_trivially_copyable_v<SubMesh>);

        dx::Buffer submesh{};


        // meshlod id == bin id in gpu
        struct MeshLod {
            uint32_t submesh_offset = 0;
            uint32_t submesh_count = 0;
            float lod_error = 0.0f;
            float next_lod_error = std::numeric_limits<float>::infinity();
        };
        static_assert(sizeof(MeshLod) == 16);
        static_assert(std::is_trivially_copyable_v<MeshLod>);

        dx::Buffer mesh_lod{};


        struct Mesh {
            DirectX::XMFLOAT3 bounds_center{};
            float bounds_radius = 0.0f;

            uint32_t lod_offset = 0;
            uint32_t lod_count = 0;
            uint32_t impostor_card_lod_offset = Consts::IND_ERR;
            uint32_t impostor_direction_count = 0;
        };
        static_assert(sizeof(Mesh) == 32);
        static_assert(std::is_trivially_copyable_v<Mesh>);

        dx::Buffer mesh{};


        struct SpatialCluster {
            DirectX::XMFLOAT3 bounds_center{};
            float bounds_radius = 0.0f;

            uint32_t mesh_id = Consts::IND_ERR;
            uint32_t instance_offset = Consts::IND_ERR;
            uint32_t instance_count = 0;
            uint32_t padding = 0;
        };
        static_assert(sizeof(SpatialCluster) == 32);
        static_assert(std::is_trivially_copyable_v<SpatialCluster>);


        dx::Buffer spatial_cluster{};


        uint32_t instance_count = 0;
        uint32_t spatial_cluster_count = 0;
        uint32_t mesh_lod_count = 0;
        uint32_t submesh_count = 0;

        static DataPersistent build(
            const scene::StaticScene& scene,
            ID3D12Device* device,
            dx::ResourceUploader& uploader,
            dx::DescriptorHeap& heap_srv_cbv_uav,
            dx::DescriptorHeap& heap_sampler);
    };
}
