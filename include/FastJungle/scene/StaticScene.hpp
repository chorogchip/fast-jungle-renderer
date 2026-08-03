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

		tangent.xyz = tangent direction
		tangent.w = bitangent sign
		quarternion : (x, y, z, w)

		clip space - x, y: -1 ~ +1 / z: 0 ~ +1
		depth: near: 0 / far: 1
		viewport: origin: top-left / +X: right / +Y: down
		uv: origin: top-left / +X: right / +Y: down

		Final mesh transform:
		PrototypePart::local_transform * PointInstanceTRS * PointBatch::local_to_world
		
		MatrixInstance::transform is already a world transform:
		PrototypePart::local_transform * MatrixInstance::transform
		*/

		struct Vertex {
			DirectX::XMFLOAT3 position{};
			DirectX::XMFLOAT3 normal{};
			DirectX::XMFLOAT4 tangent{};
			DirectX::XMFLOAT2 uv{};
		};

		static_assert(sizeof(Vertex) == 48);

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

			uint32_t texture_binding_base_color = INVALID_INDEX;
			uint32_t texture_binding_normal = INVALID_INDEX;
			uint32_t texture_binding_roughness = INVALID_INDEX;
			uint32_t texture_binding_opacity = INVALID_INDEX;
			uint32_t texture_binding_emissive = INVALID_INDEX;
		};

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
			math::AABB local_bounds{};
			EnumSubmeshFlag flags = EnumSubmeshFlag::DEFAULT;
		};

		struct Mesh {
			uint32_t name = INVALID_INDEX;
			uint32_t submesh_offset = INVALID_INDEX;
			uint32_t submesh_count = 0;
			math::AABB local_bounds{};
		};

		struct PrototypePart {
			uint32_t mesh = INVALID_INDEX;
			DirectX::XMFLOAT4X4 local_transform = IDENTITY_TRANSFORM;
		};

		enum class EnumObjectKind : uint32_t {
			UNKNOWN,
			PYRAMID,
			RIVER,
			CREEK,
			TERRAIN,
			BANYAN,
			ANTHURIUM,
			GRASS_A,
			GRASS_B,
			PYRAMID_GRASS_B,
			PYRAMID_MOSS,
			QUEEN_FOREST,
			RIVER_FOREST,
			RIVER_SAPLING,
			RIVER_SEEDLING,
			SHRUB,
			SHRUB_SORREL,
			NETTLE,
		};

		struct Prototype {
			uint32_t name = INVALID_INDEX;
			EnumObjectKind object_kind = EnumObjectKind::UNKNOWN;
			uint32_t part_offset = INVALID_INDEX;
			uint32_t part_count = 0;
			math::AABB local_bounds{};
		};

		struct PointInstance {
			DirectX::XMFLOAT3 position{};
			DirectX::XMFLOAT4 orientation{ 0.0f, 0.0f, 0.0f, 1.0f};
			DirectX::XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };
		};
		static_assert(sizeof(PointInstance) == 40);

		struct PointBatch {
			uint32_t name = INVALID_INDEX;
			uint32_t prototype = INVALID_INDEX;
			uint32_t instance_offset = INVALID_INDEX;
			uint32_t instance_count = 0;
			DirectX::XMFLOAT4X4 local_to_world = IDENTITY_TRANSFORM;
			math::AABB world_bounds{};
		};

		struct MatrixInstance {
			DirectX::XMFLOAT4X4 transform = IDENTITY_TRANSFORM;
		};

		struct MatrixBatch {
			uint32_t name = INVALID_INDEX;
			uint32_t prototype = INVALID_INDEX;
			uint32_t instance_offset = INVALID_INDEX;
			uint32_t instance_count = 0;
			math::AABB world_bounds{};
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
			math::AABB world_bounds{};
		};

		// ascii null-terminated strings.
		// Every name is a byte offset into strings.
		// strings[0] is '\0'; offset 0 represents an empty name.
		std::vector<char> strings;

		std::vector<Vertex> vertices;

		// indices[index_offset ...] are local to Submesh::vertex_offset.
		// Final vertex index = Submesh::vertex_offset + indices[i].
		std::vector<uint32_t> indices;

		std::vector<Sampler> samplers;
		std::vector<std::byte> texture_data;
		std::vector<TextureMip> texture_mips;
		std::vector<Texture> textures;
		std::vector<TextureBinding> texture_bindings;
		std::vector<Material> materials;

		std::vector<Submesh> submeshes;
		std::vector<Mesh> meshes;

		std::vector<PrototypePart> prototype_parts;
		std::vector<Prototype> prototypes;

		std::vector<PointInstance> point_instances;
		std::vector<PointBatch> point_batches;
		std::vector<MatrixInstance> matrix_instances;
		std::vector<MatrixBatch> matrix_batches;

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
		static_assert(std::is_trivially_copyable_v<Mesh>);
		static_assert(std::is_trivially_copyable_v<PrototypePart>);
		static_assert(std::is_trivially_copyable_v<Prototype>);

		static_assert(std::is_trivially_copyable_v<PointInstance>);
		static_assert(std::is_trivially_copyable_v<PointBatch>);
		static_assert(std::is_trivially_copyable_v<MatrixInstance>);
		static_assert(std::is_trivially_copyable_v<MatrixBatch>);

		static_assert(std::is_trivially_copyable_v<Camera>);
		static_assert(std::is_trivially_copyable_v<EnvironmentLight>);
		static_assert(std::is_trivially_copyable_v<SceneInfo>);

		static_assert(std::is_trivially_copyable_v<math::AABB>);


		static_assert(std::is_standard_layout_v<Vertex>);

		static_assert(std::is_standard_layout_v<Sampler>);
		static_assert(std::is_standard_layout_v<TextureMip>);
		static_assert(std::is_standard_layout_v<Texture>);
		static_assert(std::is_standard_layout_v<TextureBinding>);
		static_assert(std::is_standard_layout_v<Material>);

		static_assert(std::is_standard_layout_v<Submesh>);
		static_assert(std::is_standard_layout_v<Mesh>);
		static_assert(std::is_standard_layout_v<PrototypePart>);
		static_assert(std::is_standard_layout_v<Prototype>);

		static_assert(std::is_standard_layout_v<PointInstance>);
		static_assert(std::is_standard_layout_v<PointBatch>);
		static_assert(std::is_standard_layout_v<MatrixInstance>);
		static_assert(std::is_standard_layout_v<MatrixBatch>);

		static_assert(std::is_standard_layout_v<Camera>);
		static_assert(std::is_standard_layout_v<EnvironmentLight>);
		static_assert(std::is_standard_layout_v<SceneInfo>);

		static_assert(std::is_standard_layout_v<math::AABB>);
	};
}