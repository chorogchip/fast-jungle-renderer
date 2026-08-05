#pragma once

#include <d3d12.h>

#include "FastJungle/dx12/ResourceUploader.hpp"
#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/data/SceneResources.hpp"

namespace fjr::render {

    class SceneTextureResourcesBuilder final {
    public:
        SceneTextureResourcesBuilder() = delete;

        static void build(
            data::SceneResources::MaterialResources& output,
            dx::ResourceUploader& uploader,
            ID3D12Device* device,
            const scene::StaticScene& scene);
    };

} // namespace fjr::render