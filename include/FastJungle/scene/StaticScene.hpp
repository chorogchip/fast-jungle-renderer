#pragma once

#include <vector>
#include <cstdint>
#include <DirectXMath.h>

#include "FastJungle/core/math/AABB.hpp"

namespace fjr::scene {

	class StaticScene {

	public:
		struct Vertex {
			float position[3];
			float normal[3];
			float tangent[4];
			float uv[2];
		};
		static_assert(sizeof(Vertex) == 48);

		struct Material {
			// TODO
		};

		struct Submesh {
			uint32_t vertex_offset;
			uint32_t vertex_count;
			uint32_t index_offset;
			uint32_t index_count;
			uint32_t material;
			math::AABB local_bound;
		};

		struct Mesh {
			uint32_t submesh_offset;
			uint32_t submesh_count;
			math::AABB local_bound;
		};

		struct Instance {
			uint32_t mesh;
			DirectX::XMFLOAT3X4 world_transform;
			math::AABB world_bound;
		};

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices_local;
		std::vector<Material> materials;
		std::vector<Submesh> submeshes;
		std::vector<Mesh> meshes;
		std::vector<Instance> instances;

		uint32_t instnace_pyramid;
	};
}