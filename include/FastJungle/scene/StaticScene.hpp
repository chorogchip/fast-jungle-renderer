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

		struct Vertex {
			DirectX::XMFLOAT3 position{};
			DirectX::XMFLOAT3 normal{};
			DirectX::XMFLOAT4 tangent{};
			DirectX::XMFLOAT2 uv{};
		};
		static_assert(sizeof(Vertex) == 48);

		struct Sampler {
			uint32_t name = INVALID_INDEX;
			uint32_t filter = 0;  // 0 linear
			uint32_t address_u = 0;  // 0 wrap
			uint32_t address_v = 0;
			uint32_t address_w = 0;
			uint32_t max_anisotropy = 1;
		};

		struct Texture {
			uint32_t name = INVALID_INDEX;
			uint32_t width = 1;
			uint32_t height = 1;
			uint32_t dxgi_format = 0;
			// uint32_t mip_count = 1;  // mip count is 1 in first implementation
			uint32_t row_pitch = 0;
			uint32_t data_byte_offset = INVALID_INDEX;  // safe in 4GB
			uint32_t data_size = 0;
		};

		struct TextureBinding {
			uint32_t name = INVALID_INDEX;
			uint32_t texture = INVALID_INDEX;
			uint32_t sampler = INVALID_INDEX;
			uint32_t channel = INVALID_INDEX;
			uint32_t flags = 0;
		};

		struct Material {
			uint32_t name = INVALID_INDEX;
			DirectX::XMFLOAT4 base_color{};
			DirectX::XMFLOAT4 emissive_roughness{};
			DirectX::XMFLOAT4 surface{};
			DirectX::XMUINT4 options{};
			uint32_t texture_binding_base_color = INVALID_INDEX;
			uint32_t texture_binding_normal = INVALID_INDEX;
			uint32_t texture_binding_roughness = INVALID_INDEX;
			uint32_t texture_binding_opacity = INVALID_INDEX;
			uint32_t flags = 0;  // no usage yet
		};

		struct Submesh {
			uint32_t name = INVALID_INDEX;
			uint32_t vertex_offset = INVALID_INDEX;
			uint32_t vertex_count = 0;
			uint32_t index_offset = INVALID_INDEX;
			uint32_t index_count = 0;
			uint32_t material = INVALID_INDEX;
			uint32_t flags = 0;  // 1: double sided, 2: alpha tested
		};

		struct Mesh {
			uint32_t name = INVALID_INDEX;
			uint32_t submesh_offset = INVALID_INDEX;
			uint32_t submesh_count = 0;
		};

		struct Instance {
			uint32_t name = INVALID_INDEX;
			uint32_t mesh = INVALID_INDEX;
		};

		struct Node {
			uint32_t name = INVALID_INDEX;
			uint32_t instance_optional = INVALID_INDEX;
			uint32_t child_node_offset = INVALID_INDEX;
			uint32_t child_node_count = 0;  // childs are subsequent in node vector
			DirectX::XMFLOAT4X4 local_transform{};
		};

		std::vector<char> names;
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices_local;
		std::vector<Sampler> samplers;
		std::vector<std::byte> texture_datas;
		std::vector<Texture> textures;
		std::vector<TextureBinding> texture_bindings;
		std::vector<Material> materials;
		std::vector<Submesh> submeshes;
		std::vector<Mesh> meshes;
		std::vector<Instance> instances;
		std::vector<Node> nodes;

		uint32_t root_node_pyramid = INVALID_INDEX;

		static_assert(std::is_trivially_copyable_v<Vertex>);
		static_assert(std::is_trivially_copyable_v<Sampler>);
		static_assert(std::is_trivially_copyable_v<Texture>);
		static_assert(std::is_trivially_copyable_v<TextureBinding>);
		static_assert(std::is_trivially_copyable_v<Material>);
		static_assert(std::is_trivially_copyable_v<Submesh>);
		static_assert(std::is_trivially_copyable_v<Mesh>);
		static_assert(std::is_trivially_copyable_v<Instance>);
		static_assert(std::is_trivially_copyable_v<Node>);

		static_assert(std::is_trivially_copyable_v<math::AABB>);
		static_assert(std::is_standard_layout_v<math::AABB>);
	};
}