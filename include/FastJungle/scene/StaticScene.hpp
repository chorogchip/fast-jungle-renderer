#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <DirectXMath.h>

#include "FastJungle/core/math/AABB.hpp"

namespace fjr::scene {

	class StaticScene {

	public:
		static constexpr inline uint32_t INVALID_INDEX = UINT32_MAX;
		static constexpr inline uint64_t INVALID_INDEX_64 = UINT64_MAX;
		static constexpr DirectX::XMFLOAT4X4 IDENTITY_TRANSFORM{
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};

		/*
		+X = right
		+Y = up
		+Z = forward
		left-handed

		1 meter per unit
		row vector convention, mul(vec, mat)
		row major matrix store
		transform: vec * S * R * T (row-vector)
		world = local * parent_world

		Front_CCW = false (default CW front fase)

		quarternion : (x, y, z, w)

		clip space - x, y: -1 ~ +1 / z: 0 ~ +1
		depth: near: 0 / far: 1
		viewport: origin: top-left / +X: right / +Y: down
		uv: origin: top-left / +X: right / +Y: down

		Final point mesh transform:
		PointMeshBatch::local_transform * PointInstanceTRS
		
		StaticMeshInstance::world_transform is the final world transform.
		*/

		struct IndexRange {
			uint32_t offset = INVALID_INDEX;
			uint32_t count = 0;
		};

		struct Vertex {
			DirectX::XMFLOAT3 position{};
			DirectX::XMFLOAT3 normal{};
			DirectX::XMFLOAT2 uv{};
		};

		static_assert(sizeof(Vertex) == 32);

		enum class EnumSamplerFilter : uint32_t {
			MIN_MAG_MIP_POINT = 0x000,
			MIN_MAG_POINT_MIP_LINEAR = 0x001,
			MIN_POINT_MAG_LINEAR_MIP_POINT = 0x004,
			MIN_POINT_MAG_MIP_LINEAR = 0x005,
			MIN_LINEAR_MAG_MIP_POINT = 0x010,
			MIN_LINEAR_MAG_POINT_MIP_LINEAR = 0x011,
			MIN_MAG_LINEAR_MIP_POINT = 0x014,
			MIN_MAG_MIP_LINEAR = 0x015,
			ANISOTROPIC = 0x055,
		};

		enum class EnumSamplerAddressMode : uint32_t {
			WRAP = 1,
			MIRROR = 2,
			CLAMP = 3,
			BORDER = 4,
			MIRROR_ONCE = 5,
		};

		struct Sampler {
			EnumSamplerFilter filter = EnumSamplerFilter::MIN_MAG_POINT_MIP_LINEAR;
			EnumSamplerAddressMode address_u = EnumSamplerAddressMode::WRAP;
			EnumSamplerAddressMode address_v = EnumSamplerAddressMode::WRAP;
			EnumSamplerAddressMode address_w = EnumSamplerAddressMode::WRAP;
			uint32_t max_anisotropy = 1;
		};

		// Texture::data_byte_offset:
		//   absolute offset in texture_data.
		//
		// TextureMip::data_byte_offset_local:
		//   offset relative to Texture::data_byte_offset.
		//
		// Texture::data_size:
		//   complete byte range occupied by all mips of this texture.

		struct TextureMip {
			uint32_t width = 0;
			uint32_t height = 0;
			uint32_t row_pitch = 0;
			uint32_t slice_pitch = 0;
			uint64_t data_byte_offset_local = INVALID_INDEX_64;  // starts from texture's local
		};

		struct Texture {
			uint32_t name = INVALID_INDEX;
			uint32_t width = 0;
			uint32_t height = 0;
			uint32_t dxgi_format = 0;
			uint32_t mip_offset = INVALID_INDEX;
			uint32_t mip_count = 0;
			uint64_t data_byte_offset = INVALID_INDEX_64;
			uint64_t data_size = 0;
		};

		enum class EnumTextureChannel : uint32_t {
			RGBA,
			R,
			G,
			B,
			A,
			RGB,
		};

		enum class EnumTextureBindingFlag : uint32_t {
			LINEAR = 0,
			SRGB = 1u << 0,
		};

		struct TextureBinding {
			uint32_t texture = INVALID_INDEX;
			uint32_t sampler = INVALID_INDEX;
			EnumTextureChannel channel = EnumTextureChannel::RGBA;
			EnumTextureBindingFlag flags = EnumTextureBindingFlag::LINEAR;
		};

		struct Material {
			uint32_t name = INVALID_INDEX;
			DirectX::XMFLOAT4 base_color{ 0.18f, 0.18f, 0.18f, 1.0f };
			DirectX::XMFLOAT3 emissive{};
			float roughness = 0.5f;
			float metallic = 0.0f;
			float opacity = 1.0f;
			float opacity_threshold = 0.0f;
			float ior = 1.5f;
			float specular = 0.5f;
			float clearcoat = 0.0f;
			float clearcoat_roughness = 0.01f;

			uint32_t texture_binding_base_color = INVALID_INDEX;
			uint32_t texture_binding_normal = INVALID_INDEX;
			uint32_t texture_binding_roughness = INVALID_INDEX;
			uint32_t texture_binding_metallic = INVALID_INDEX;
			uint32_t texture_binding_opacity = INVALID_INDEX;
			uint32_t texture_binding_emissive = INVALID_INDEX;
		};
		static_assert(sizeof(Material) == 88);

		enum class EnumSubmeshFlag : uint32_t {
			DEFAULT = 0,
			DOUBLE_SIDED = 1u << 0,
			ALPHA_TESTED = 1u << 1,
			ALPHA_BLENDED = 1u << 2,

			DOUBLE_SIDED_AND_ALPHA_TESTED =
			DOUBLE_SIDED | ALPHA_TESTED,

			DOUBLE_SIDED_AND_ALPHA_BLENDED =
			DOUBLE_SIDED | ALPHA_BLENDED,
		};

		struct Submesh {
			uint32_t name = INVALID_INDEX;
			uint32_t vertex_offset = INVALID_INDEX;
			uint32_t vertex_count = 0;
			uint32_t index_offset = INVALID_INDEX;
			uint32_t index_count = 0;
			uint32_t material = INVALID_INDEX;
			EnumSubmeshFlag flags = EnumSubmeshFlag::DEFAULT;
		};

		struct MeshLod {
			uint32_t submesh_offset = INVALID_INDEX;
			uint32_t submesh_count = 0;
			// Accumulated maximum deviation from LOD0 in mesh-local meters.
			float max_deviation = 0.0f;
			uint32_t reserved = 0;
		};

		struct Mesh {
			uint32_t name = INVALID_INDEX;
			uint32_t lod_offset = INVALID_INDEX;
			uint32_t lod_count = 0;
		};

		enum class EnumAttributeInterpolation : uint32_t {
			CONSTANT,
			UNIFORM,
			VERTEX,
			VARYING,
			FACE_VARYING,
		};

		// Auxiliary primvars are expanded by the Cooker into the final triangle
		// order. This preserves values without retaining USD topology or asking a
		// later compiler to resolve interpolation.
		struct TriangleBoolStream {
			uint32_t mesh = INVALID_INDEX;
			uint32_t name = INVALID_INDEX;
			uint32_t value_offset = INVALID_INDEX;
			uint32_t value_count = 0;
		};

		struct CornerFloatStream {
			uint32_t mesh = INVALID_INDEX;
			uint32_t name = INVALID_INDEX;
			EnumAttributeInterpolation source_interpolation =
				EnumAttributeInterpolation::CONSTANT;
			uint32_t value_offset = INVALID_INDEX;
			uint32_t value_count = 0;
		};

		struct CornerColor3Stream {
			uint32_t mesh = INVALID_INDEX;
			uint32_t name = INVALID_INDEX;
			EnumAttributeInterpolation source_interpolation =
				EnumAttributeInterpolation::CONSTANT;
			uint32_t value_offset = INVALID_INDEX;
			uint32_t value_count = 0;
		};

		struct CornerTexcoord2Stream {
			uint32_t mesh = INVALID_INDEX;
			uint32_t name = INVALID_INDEX;
			EnumAttributeInterpolation source_interpolation =
				EnumAttributeInterpolation::CONSTANT;
			uint32_t value_offset = INVALID_INDEX;
			uint32_t value_count = 0;
		};

		struct PointInstance {
			DirectX::XMFLOAT3 position{};
			DirectX::XMFLOAT4 orientation{ 0.0f, 0.0f, 0.0f, 1.0f};
			DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };
		};
		static_assert(sizeof(PointInstance) == 40);

		enum class EnumPointCategory : uint32_t {
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
			COUNT,
		};

		struct PointCategorySpan {
			EnumPointCategory category = EnumPointCategory::ANTHURIUM;
			IndexRange instances;
		};

		struct PointMeshBatch {
			uint32_t mesh = INVALID_INDEX;
			IndexRange category_spans;
			DirectX::XMFLOAT4X4 local_transform = IDENTITY_TRANSFORM;
		};
		static_assert(sizeof(PointCategorySpan) == 12);
		static_assert(sizeof(PointMeshBatch) == 76);

		struct StaticMeshInstance {
			uint32_t name = INVALID_INDEX;
			uint32_t mesh = INVALID_INDEX;
			DirectX::XMFLOAT4X4 world_transform = IDENTITY_TRANSFORM;
		};

		struct Pyramid { uint32_t instance = INVALID_INDEX; };
		struct River { uint32_t instance = INVALID_INDEX; };
		struct Creek { uint32_t instance = INVALID_INDEX; };
		struct Banyan { uint32_t instance = INVALID_INDEX; };
		struct Terrain {
			IndexRange extended;
			IndexRange cinematic;
		};

		// These members are the compiler-visible contract of the Jungle root
		// USDA. Counts inside a component remain data; component identity and
		// storage shape do not.
		struct Components {
			Pyramid pyramid;
			River river;
			Creek creek;
			Banyan banyan;
			Terrain terrain;
		};

		struct Camera {
			uint32_t name = INVALID_INDEX;
			DirectX::XMFLOAT4X4 world_transform = IDENTITY_TRANSFORM;
			float focal_length = 0.0f;
			float horizontal_aperture = 0.0f;
			float vertical_aperture = 0.0f;
			float horizontal_aperture_offset = 0.0f;
			float vertical_aperture_offset = 0.0f;
			float focus_distance = 0.0f;
			float f_stop = 0.0f;
			DirectX::XMFLOAT2 clipping_range{};
		};

		struct EnvironmentLight {
			uint32_t name = INVALID_INDEX;
			DirectX::XMFLOAT4X4 world_transform = IDENTITY_TRANSFORM;
			DirectX::XMFLOAT3 color{};
			float intensity = 0.0f;
			float exposure = 0.0f;
			uint32_t texture = INVALID_INDEX;
		};

		struct SceneInfo {
			// Number of triangle corners before and unique vertices after
			// submesh-local position/normal/UV indexing.
			uint64_t vertex_count_before_indexing = 0;
			uint64_t vertex_count_after_indexing = 0;
		};

		// strings is ascii null-terminated strings.
		// Every name is a byte offset into strings.
		// strings[0] is '\0'; offset 0 represents an empty name.


		// indices[index_offset ...] are local to Submesh::vertex_offset.
		// Final vertex index = Submesh::vertex_offset + indices[i].

		using Char = char;
		using Uint32_t = uint32_t;
		using Uint8_t = uint8_t;
		using Float = float;
		using Float2 = DirectX::XMFLOAT2;
		using Float3 = DirectX::XMFLOAT3;
		using Byte = std::byte;

#define SceneDataBeforeTexture_MACRO \
    X(Char, strings) \
    X(Vertex, vertices) \
    X(Uint32_t, indices) \
    \
    X(Sampler, samplers)

#define SceneDataAfterTexture_MACRO \
    X(TextureMip, texture_mips) \
    X(Texture, textures) \
    X(TextureBinding, texture_bindings) \
    X(Material, materials) \
    \
    X(Submesh, submeshes) \
    X(MeshLod, mesh_lods) \
    X(Mesh, meshes) \
    X(TriangleBoolStream, triangle_bool_streams) \
    X(Uint8_t, triangle_bool_values) \
    X(CornerFloatStream, corner_float_streams) \
    X(Float, corner_float_values) \
    X(CornerColor3Stream, corner_color3_streams) \
    X(Float3, corner_color3_values) \
    X(CornerTexcoord2Stream, corner_texcoord2_streams) \
    X(Float2, corner_texcoord2_values) \
    X(PointInstance, point_instances) \
    X(PointCategorySpan, point_category_spans) \
    X(PointMeshBatch, point_mesh_batches) \
    X(StaticMeshInstance, static_mesh_instances)

#define SceneData_MACRO \
    SceneDataBeforeTexture_MACRO \
    SceneDataAfterTexture_MACRO

#define X(type, name) std::vector<type> name;
		SceneData_MACRO
#undef X
		std::vector<Byte> texture_data;

		Components components;
		Camera camera;
		EnvironmentLight environment_light;
		SceneInfo info;

		static_assert(std::is_trivially_copyable_v<Vertex>);

		static_assert(std::is_trivially_copyable_v<Sampler>);
		static_assert(std::is_trivially_copyable_v<TextureMip>);
		static_assert(std::is_trivially_copyable_v<Texture>);
		static_assert(std::is_trivially_copyable_v<TextureBinding>);
		static_assert(std::is_trivially_copyable_v<Material>);

		static_assert(std::is_trivially_copyable_v<Submesh>);
		static_assert(std::is_trivially_copyable_v<MeshLod>);
		static_assert(std::is_trivially_copyable_v<Mesh>);
		static_assert(std::is_trivially_copyable_v<TriangleBoolStream>);
		static_assert(std::is_trivially_copyable_v<CornerFloatStream>);
		static_assert(std::is_trivially_copyable_v<CornerColor3Stream>);
		static_assert(std::is_trivially_copyable_v<CornerTexcoord2Stream>);
		static_assert(std::is_trivially_copyable_v<PointInstance>);
		static_assert(std::is_trivially_copyable_v<PointCategorySpan>);
		static_assert(std::is_trivially_copyable_v<PointMeshBatch>);
		static_assert(std::is_trivially_copyable_v<StaticMeshInstance>);
		static_assert(std::is_trivially_copyable_v<Components>);

		static_assert(std::is_trivially_copyable_v<Camera>);
		static_assert(std::is_trivially_copyable_v<EnvironmentLight>);
		static_assert(std::is_trivially_copyable_v<SceneInfo>);

		static_assert(std::is_standard_layout_v<Vertex>);

		static_assert(std::is_standard_layout_v<Sampler>);
		static_assert(std::is_standard_layout_v<TextureMip>);
		static_assert(std::is_standard_layout_v<Texture>);
		static_assert(std::is_standard_layout_v<TextureBinding>);
		static_assert(std::is_standard_layout_v<Material>);

		static_assert(std::is_standard_layout_v<Submesh>);
		static_assert(std::is_standard_layout_v<MeshLod>);
		static_assert(std::is_standard_layout_v<Mesh>);
		static_assert(std::is_standard_layout_v<TriangleBoolStream>);
		static_assert(std::is_standard_layout_v<CornerFloatStream>);
		static_assert(std::is_standard_layout_v<CornerColor3Stream>);
		static_assert(std::is_standard_layout_v<CornerTexcoord2Stream>);
		static_assert(std::is_standard_layout_v<PointInstance>);
		static_assert(std::is_standard_layout_v<PointCategorySpan>);
		static_assert(std::is_standard_layout_v<PointMeshBatch>);
		static_assert(std::is_standard_layout_v<StaticMeshInstance>);
		static_assert(std::is_standard_layout_v<Components>);

		static_assert(std::is_standard_layout_v<Camera>);
		static_assert(std::is_standard_layout_v<EnvironmentLight>);
		static_assert(std::is_standard_layout_v<SceneInfo>);

	};
}
