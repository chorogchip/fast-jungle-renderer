#pragma once

#include <cstdint>
#include <memory>

#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/SceneResources.hpp"

namespace fjr::render {

	class SceneResourcesBuilder {

	public:
		// The scene is expected to have passed StaticScene validation.
		// MatrixInstance transforms must be affine rigid transforms or use
		// uniform scale; the forward pixel shader renormalizes the normal.
		std::unique_ptr<SceneResources> build(
			ID3D12Device* device,
			ID3D12GraphicsCommandList* command_list,
			std::uint32_t frame_count,
			const scene::StaticScene& scene);
	};
}
