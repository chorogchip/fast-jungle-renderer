#pragma once

#include <d3d12.h>
#include <cstdint>
#include <memory>
#include <array>

#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/SceneResources.hpp"
#include "FastJungle/dx12/CommandContext.hpp"

namespace fjr::render {

	class SceneResourcesBuilder {

	public:
		struct BuildContexts {
			ID3D12Device* device;
			ID3D12CommandList* context;
		};

		static std::unique_ptr<SceneResources> build(
			BuildContexts& contexts,
			const scene::StaticScene& scene);
	};
}
