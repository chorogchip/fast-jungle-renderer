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

namespace fjr::render {

    class SceneResources {
    public:
        static constexpr std::uint32_t CONSTANT_BUFFER_ALIGNMENT =
            D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;

        enum class InstanceKind : std::uint32_t {
            POINT,
            MATRIX,
        };

        // Renderer-local identity for the compiler-known Jungle components.
        // This is derived from StaticScene::Components; it is not a generic
        // cooked tag and cannot create component kinds at runtime.
        enum class Component : std::uint32_t {
            PYRAMID,
            RIVER,
            CREEK,
            BANYAN,
            TERRAIN,
            ANTHURIUM,
            NETTLE,
            SHRUB_SORREL,
            SHRUB,
            GRASS_B,
            GRASS_A,
            PYRAMID_GRASS_B,
            PYRAMID_MOSS,
            QUEEN_FOREST,
            RIVER_FOREST,
            RIVER_SAPLING,
            RIVER_SEEDLING,
            INVALID = UINT32_MAX,
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

        struct DrawItem {
            InstanceKind instance_kind = InstanceKind::POINT;
            Component component = Component::INVALID;

            std::uint32_t index_count = 0;
            std::uint32_t first_index = 0;
            std::int32_t base_vertex = 0;

            std::uint32_t instance_count = 0;
            DrawConstants constants{};

            // Index of a 256-byte-aligned record in either
            // buf_cbuffer_point or buf_cbuffer_matrix.
            std::uint32_t transform_constant_index = 0;

            // Index into point_batch_bounds or static_instance_bounds,
            // selected by instance_kind. Keeping this renderer-derived index
            // avoids reconstructing semantic ownership from buffer offsets.
            std::uint32_t bounds_index = scene::StaticScene::INVALID_INDEX;

            scene::StaticScene::EnumSubmeshFlag flags =
                scene::StaticScene::EnumSubmeshFlag::DEFAULT;
        };

        // GPU mirror of the stable draw order. The initial visibility resolve
        // only consumes material_id; the remaining fields keep the buffer
        // ready for later index/vertex/instance attribute reconstruction.
        struct DrawData {
            std::uint32_t first_index = 0;
            std::int32_t base_vertex = 0;
            std::uint32_t instance_offset = 0;
            std::uint32_t material_id = scene::StaticScene::INVALID_INDEX;
            std::uint32_t instance_kind = 0;
            std::uint32_t transform_constant_index = 0;
            std::uint32_t padding[2]{};
        };

        dx::Buffer buf_vertices;
        dx::Buffer buf_indices;

        // Point instances retain the compact position/orientation/scale
        // representation. Matrix instances retain one matrix per instance.
        dx::Buffer buf_instances_matrix;
        dx::Buffer buf_instances_point;

        dx::Buffer buf_materials;
        dx::Buffer buf_texture_bindings;
        dx::Buffer buf_draw_data;

        // Point/matrix records are immutable scene data indexed by
        // DrawItem::transform_constant_index.
        dx::Buffer buf_cbuffer_matrix;
        dx::Buffer buf_cbuffer_point;

        dx::CBufferArrayView view_cbuf_transform_matrix;
        dx::CBufferArrayView view_cbuf_transform_point;

        // Temporary upload resources stay alive until the initialization
        // command list has completed on the GPU.
        std::vector<dx::Buffer> upload_buffers;

        D3D12_VERTEX_BUFFER_VIEW view_vertices{};
        D3D12_INDEX_BUFFER_VIEW view_indices{};

        std::vector<dx::Texture> textures;
        std::vector<DrawItem> draw_items;

        // CPU copies are retained until the backing constant buffers have
        // been uploaded. They also make the immutable draw layout available
        // without mapping a GPU resource. The upload abstraction can consume
        // and release these vectors later.
        std::vector<PointDrawConstants> point_draw_constants;
        std::vector<MatrixDrawConstants> matrix_draw_constants;
        std::vector<MatrixInstance> matrix_instance_data;
        std::vector<Material> material_data;
        std::vector<TextureBinding> texture_binding_data;
        std::vector<DrawData> draw_data;

        // CPU-side scene defaults used to initialize frame constants.
        scene::StaticScene::EnvironmentLight environment_light;

        std::uint32_t default_material_id = 0;


        dx::DescAlloc texture_descriptors;
        dx::DescAlloc sampler_descriptors;
    };

    static_assert(sizeof(SceneResources::PointDrawConstants) ==
        SceneResources::CONSTANT_BUFFER_ALIGNMENT);
    static_assert(sizeof(SceneResources::MatrixDrawConstants) ==
        SceneResources::CONSTANT_BUFFER_ALIGNMENT);
    static_assert(sizeof(SceneResources::DrawConstants) == 3 * sizeof(std::uint32_t));
    static_assert(sizeof(SceneResources::TextureBinding) == 16);
    static_assert(sizeof(SceneResources::Material) == 96);
    static_assert(sizeof(SceneResources::DrawData) == 32);
    static_assert(std::is_trivially_copyable_v<SceneResources::PointDrawConstants>);
    static_assert(std::is_trivially_copyable_v<SceneResources::MatrixDrawConstants>);
    static_assert(std::is_trivially_copyable_v<SceneResources::MatrixInstance>);
    static_assert(std::is_trivially_copyable_v<SceneResources::DrawConstants>);
    static_assert(std::is_trivially_copyable_v<SceneResources::TextureBinding>);
    static_assert(std::is_trivially_copyable_v<SceneResources::Material>);
    static_assert(std::is_trivially_copyable_v<SceneResources::DrawData>);

} // namespace fjr::render
