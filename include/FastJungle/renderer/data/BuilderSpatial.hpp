#pragma once

#include <d3d12.h>

#include <cstdint>
#include <span>

#include "FastJungle/dx12/ResourceUploader.hpp"
#include "FastJungle/renderer/data/DataPersistent.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render::data {

    class BuilderSpatial final {
    public:
        BuilderSpatial() = delete;

        struct Result {
            uint32_t instance_count = 0;
            uint32_t spatial_cluster_count = 0;
        };

        [[nodiscard]]
        static Result build(
            data::DataPersistent::Fixed& output,
            dx::ResourceUploader& uploader,
            ID3D12Device* device,
            const scene::StaticScene& scene,
            std::span<const data::DataPersistent::Fixed::Mesh> meshes);
    };

} // namespace fjr::render