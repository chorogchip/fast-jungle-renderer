#pragma once

#include <span>
#include <vector>

#include "FastJungle/renderer/data/DataPersistent.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render::data::mat {

    class BuilderMatTable final {
    public:
        BuilderMatTable() = delete;

        static std::vector<DataPersistent::Material> build(
            const scene::StaticScene& scene,
            std::span<const DataPersistent::Mesh> meshes);
    };

} // namespace fjr::render::data::mat
