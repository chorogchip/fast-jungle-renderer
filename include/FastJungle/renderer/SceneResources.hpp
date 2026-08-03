#pragma once

#include <vector>

#include "FastJungle/dx12/Buffer.hpp"
#include "FastJungle/dx12/Texture.hpp"

namespace fjr::render {

	class SceneResources {

	public:

		struct SubMesh {

		};

		dx::Buffer buf_vertices;
		dx::Buffer buf_indices;
		dx::Buffer buf_materials;
		dx::Buffer buf_instances_point;
		dx::Buffer buf_instances_matrix;
		dx::Buffer buf_cbuffer_camera;
		dx::Buffer buf_cbuffer_default;
		dx::Buffer buf_cbuffer_point;
		dx::Buffer buf_cbuffer_matrix;
		std::vector<dx::Texture> textures;

		dx::DescriptorHeap heap_samplers;
		dx::DescriptorHeap heap_srv;
	};
}