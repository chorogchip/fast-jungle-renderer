#pragma once

#include "FastJungle/scene/JungleScene.hpp"

namespace fjr::scene {

    class JungleSceneValidator {
    public:
        JungleSceneValidator() = delete;

        [[nodiscard]]
        static std::vector<JungleScene::Diagnostic> validate(
            const JungleScene& scene);
    };

} // namespace fjr::scene
