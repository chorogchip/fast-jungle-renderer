#pragma once

#include <DirectXMath.h>
#include <d3d12.h>

#include <cstdint>
#include <type_traits>
#include <vector>

#include "FastJungle/dx12/Buffer.hpp"
#include "FastJungle/dx12/Texture.hpp"
#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/dx12/View.hpp"
#include "FastJungle/scene/StaticScene.hpp"

#pragma warning(push)
#pragma warning(disable: 4324)

namespace fjr::render {

    class SceneResources {
    public:
        static constexpr std::uint32_t CONSTANT_BUFFER_ALIGNMENT =
            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;

        enum class InstanceKind : std::uint32_t {
            POINT,
            MATRIX,
        };

        // PointBatch transform order (row-vector convention):
        //
        //   InstancedMeshDefinition::local_transform
        //   * PointInstance TRS
        //   * PointBatch::local_to_world
        //
        // The instance transform lies between the two common transforms, so
        // the two matrices cannot be pre-combined into one matrix.
        struct alignas(CONSTANT_BUFFER_ALIGNMENT) PointDrawConstants {
            DirectX::XMFLOAT4X4 part_local_transform =
                scene::StaticScene::IDENTITY_TRANSFORM;
            DirectX::XMFLOAT4X4 batch_local_to_world =
                scene::StaticScene::IDENTITY_TRANSFORM;
        };

        // The renderer stages StaticMeshInstance::world_transform as one
        // MatrixInstance. Its matrix is already the complete world transform.
        // Static transforms are required to be rigid transforms or
        // to contain uniform scale. The pixel shader renormalizes normals.
        struct alignas(CONSTANT_BUFFER_ALIGNMENT) MatrixDrawConstants {
            DirectX::XMFLOAT4X4 part_local_transform =
                scene::StaticScene::IDENTITY_TRANSFORM;
        };

        // GPU-only staging form. It intentionally does not live in
        // StaticScene because the cooked source of truth is
        // StaticMeshInstance::world_transform.
        struct MatrixInstance {
            DirectX::XMFLOAT4X4 transform =
                scene::StaticScene::IDENTITY_TRANSFORM;
        };

        // These values are intended for root constants. Geometry ranges and
        // instance_count stay in DrawItem because they are direct arguments
        // to DrawIndexedInstanced rather than shader inputs.
        struct DrawConstants {
            std::uint32_t instance_offset = 0;
            std::uint32_t material_id = scene::StaticScene::INVALID_INDEX;
            std::uint32_t instance_kind = 0;
        };
        struct TextureBinding {
            std::uint32_t texture_id = scene::StaticScene::INVALID_INDEX;
            std::uint32_t sampler_id = scene::StaticScene::INVALID_INDEX;
            std::uint32_t channel = 0;
            std::uint32_t flags = 0;
        };

        struct Material {
            DirectX::XMFLOAT4 base_color{
                0.18f, 0.18f, 0.18f, 1.0f};
            DirectX::XMFLOAT4 emissive_roughness{
                0.0f, 0.0f, 0.0f, 0.5f};
            DirectX::XMFLOAT4 surface{
                0.0f, 1.0f, 0.0f, 0.0f};
            DirectX::XMFLOAT4 optical{
                0.5f, 0.0f, 0.01f, 0.0f};

            // base color, normal, roughness, opacity
            DirectX::XMUINT4 texture_bindings_0{
                scene::StaticScene::INVALID_INDEX,
                scene::StaticScene::INVALID_INDEX,
                scene::StaticScene::INVALID_INDEX,
                scene::StaticScene::INVALID_INDEX};

            // emissive, metallic, reserved, reserved
            DirectX::XMUINT4 texture_bindings_1{
                scene::StaticScene::INVALID_INDEX,
                scene::StaticScene::INVALID_INDEX,
                scene::StaticScene::INVALID_INDEX,
                scene::StaticScene::INVALID_INDEX};
        };

        dx::Buffer buf_vertices;
        dx::Buffer buf_indices;

        // Point instances retain the compact position/orientation/scale
        // representation. Matrix instances retain one matrix per instance.
        dx::Buffer buf_instances_matrix;
        dx::Buffer buf_instances_point;

        dx::Buffer buf_materials;
        dx::Buffer buf_texture_bindings;

        // Point/matrix records are immutable scene data indexed by the
        // corresponding SceneDrawItem::transform_constant_index.
        dx::Buffer buf_cbuffer_matrix;
        dx::Buffer buf_cbuffer_point;

        dx::CBufferArrayView view_cbuf_transform_matrix;
        dx::CBufferArrayView view_cbuf_transform_point;

        D3D12_VERTEX_BUFFER_VIEW view_vertices{};
        D3D12_INDEX_BUFFER_VIEW view_indices{};

        std::vector<dx::Texture> textures;

        dx::DescAlloc texture_descriptors;
        dx::DescAlloc sampler_descriptors;


        // buffers for GPU indirect culling
        dx::Buffer buf_point_clusters;
        dx::Buffer buf_point_batches_gpu;
        dx::Buffer buf_point_definitions;
        dx::Buffer buf_point_draw_templates;

        uint32_t point_cluster_count = 0;
        uint32_t point_instance_count = 0;
        uint32_t point_bin_count = 0;
        uint32_t point_draw_template_count = 0;

        std::array<uint32_t, POINT_PIPELINE_CLASS_COUNT> point_command_class_bases{};

        std::array<uint32_t, POINT_PIPELINE_CLASS_COUNT> point_command_class_capacities{};
    };

    static_assert(sizeof(SceneResources::PointDrawConstants) ==
        SceneResources::CONSTANT_BUFFER_ALIGNMENT);
    static_assert(sizeof(SceneResources::MatrixDrawConstants) ==
        SceneResources::CONSTANT_BUFFER_ALIGNMENT);
    static_assert(sizeof(SceneResources::DrawConstants) == 3 * sizeof(std::uint32_t));
    static_assert(sizeof(SceneResources::TextureBinding) == 16);
    static_assert(sizeof(SceneResources::Material) == 96);
    static_assert(std::is_trivially_copyable_v<SceneResources::PointDrawConstants>);
    static_assert(std::is_trivially_copyable_v<SceneResources::MatrixDrawConstants>);
    static_assert(std::is_trivially_copyable_v<SceneResources::MatrixInstance>);
    static_assert(std::is_trivially_copyable_v<SceneResources::DrawConstants>);
    static_assert(std::is_trivially_copyable_v<SceneResources::TextureBinding>);
    static_assert(std::is_trivially_copyable_v<SceneResources::Material>);

} // namespace fjr::render

#pragma warning(pop)