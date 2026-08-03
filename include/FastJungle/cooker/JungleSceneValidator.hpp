#pragma once

#include "FastJungle/cooker/JungleScene.hpp"

namespace fjr::cooker {

    class JungleSceneValidator {
    public:
        JungleSceneValidator() = delete;

        [[nodiscard]]
        static std::vector<JungleScene::Diagnostic> validate(
            const JungleScene& scene);
    };

} // namespace fjr::scene
