#include "FastJungle/renderer/builder/ScenePreBuilder.hpp"

#include <cstdint>

#include "FastJungle/core/util/EnumUtils.hpp"

namespace fjr::render {

    void ScenePreBuilder::build(scene::StaticScene& scene) noexcept {
        using Flag = scene::StaticScene::EnumSubmeshFlag;

        for (auto& submesh : scene.submeshes) {
            auto flags = static_cast<std::uint32_t>(submesh.flags);
            if (enm::has(submesh.flags, Flag::ALPHA_TESTED)) {
                flags |= static_cast<std::uint32_t>(Flag::DOUBLE_SIDED);
            }
            else {
                flags &= ~static_cast<std::uint32_t>(Flag::DOUBLE_SIDED);
            }
            submesh.flags = static_cast<Flag>(flags);
        }
    }

} // namespace fjr::render
