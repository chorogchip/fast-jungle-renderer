#pragma once

#include <d3d12.h>

#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/dx12/ResourceUploader.hpp"
#include "FastJungle/renderer/data/DataPersistent.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render::data::mat {

    class BuilderMatTexture final {
    public:
        BuilderMatTexture() = delete;

        static void build(
            DataPersistent& output,
            dx::ResourceUploader& uploader,
            ID3D12Device* device,
            dx::DescriptorHeap& heap_srv_cbv_uav,
            const scene::StaticScene& scene);
    };

} // namespace fjr::render::data::mat
