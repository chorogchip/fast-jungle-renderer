#pragma once

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>

#include "FastJungle/core/math/AABB.hpp"
#include "FastJungle/scene/StaticScene.hpp"
#include "FastJungle/renderer/data/DataPersistent.hpp"

namespace fjr::render::data::geom {

    class BuilderGeomVertex {

    public:
        static std::vector<DataPersistent::VertexDecodeParams> build(
            std::vector<DataPersistent::PackedPosition>& dest_pos,
            std::vector<DataPersistent::PackedNormal>& dest_normal,
            std::vector<DataPersistent::PackedUV>& dest_uv,
            const scene::StaticScene& scene
        );

    };

}
