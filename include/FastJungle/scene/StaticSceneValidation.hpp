#pragma once

#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::scene {

    // Checks every index/range that the renderer dereferences. Throws
    // std::runtime_error when the scene is not safe to consume.
    void validate_static_scene(const StaticScene& scene);

    // Requires an exact in-memory round trip, including every vector and
    // fixed scene record. The serializer deliberately preserves these bytes.
    void require_static_scene_equal(
        const StaticScene& expected,
        const StaticScene& actual);

} // namespace fjr::scene
