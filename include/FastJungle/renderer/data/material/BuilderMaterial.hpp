#pragma once

#include <d3d12.h>
#include <span>

#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/dx12/ResourceUploader.hpp"
#include "FastJungle/renderer/data/DataPersistent.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render::data {

    class BuilderMaterial final {
    public:
        BuilderMaterial() = delete;

        static void build(
            data::DataPersistent& output,
            dx::ResourceUploader& uploader,
            ID3D12Device* device,
            dx::DescriptorHeap& heap_srv_cbv_uav,
            dx::DescriptorHeap& heap_sampler,
            const scene::StaticScene& scene,
            std::span<const data::DataPersistent::Mesh> meshes);
    };

} // namespace fjr::render::data
