#pragma once

#include <d3d12.h>

#include "FastJungle/dx12/DescriptorHeap.hpp"
#include "FastJungle/renderer/data/DataPersistent.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render::data::mat {

    class BuilderMatSampler final {
    public:
        BuilderMatSampler() = delete;

        static void build(
            DataPersistent& output,
            ID3D12Device* device,
            dx::DescriptorHeap& heap_sampler,
            const scene::StaticScene& scene);
    };

} // namespace fjr::render::data::mat
