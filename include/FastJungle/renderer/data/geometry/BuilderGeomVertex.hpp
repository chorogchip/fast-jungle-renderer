#pragma once

#include <cstdint>
#include <vector>

#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/data/DataPersistent.hpp"

namespace fjr::render::data::geom {

    class BuilderGeomVertex {

    public:
        struct Result {
            std::vector<DataPersistent::OpaqueVertex0> opaque_visibility;
            std::vector<DataPersistent::OpaqueVertex1> opaque_shading;
            std::vector<DataPersistent::AlphaVertex0> alpha_visibility;
            std::vector<DataPersistent::AlphaVertex1> alpha_shading;
            std::vector<DataPersistent::VertexDecodeParams> decode_params;
            std::vector<std::int32_t> submesh_base_vertices;
        };

        static Result build(const scene::StaticScene& scene);

    };

}
