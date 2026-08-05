#pragma once

#include <d3d12.h>
#include <cstdint>
#include <memory>
#include <vector>

#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/SceneRenderData.hpp"
#include "FastJungle/renderer/SceneResources.hpp"
#include "FastJungle/dx12/CommandQueue.hpp"

namespace fjr::render {

	class SceneResourcesBuilder {

	public:
		struct BuildResult {
			std::unique_ptr<SceneResources> resources;
			std::vector<SceneDrawItem> draw_items;
		};

		struct BuildContexts {
			ID3D12Device* device;
			dx::CommandQueue* command_queue;
		};

		static BuildResult build(
			BuildContexts& contexts,
			const scene::StaticScene& scene);
	};
} // namespace fjr::render
