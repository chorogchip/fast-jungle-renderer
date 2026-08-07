#pragma once

#include <vector>
#include <d3d12.h>

#include "FastJungle/dx12/ResourceUploader.hpp"
#include "FastJungle/renderer/data/DataPersistent.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render::data {

    class BuilderGeometry final {

    public:
        BuilderGeometry() = delete;

        struct Result {
            std::vector<data::DataPersistent::Fixed::Mesh> meshes;
        };

        static Result build(
            data::DataPersistent::Fixed& output,
            dx::ResourceUploader& uploader,
            ID3D12Device* device,
            const scene::StaticScene& scene);
    };

} // namespace fjr::render