#pragma once

#include <vector>

#include "FastJungle/renderer/data/DataPersistent.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::render::data::geom {

    class BuilderGeomMeshLod {

    public:
        static std::vector<DataPersistent::MeshLod> build(
            const scene::StaticScene& scene);
    };

}
