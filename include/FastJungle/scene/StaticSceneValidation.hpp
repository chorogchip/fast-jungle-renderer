#pragma once

#include <cstdint>
#include <string_view>

#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::scene {

    class StaticSceneValidator final {
    public:
        StaticSceneValidator() = delete;

        static void validate(const StaticScene& scene);

        static void validate(
            const StaticScene& scene,
            uint64_t texture_payload_size);

    private:
        static void require_index(
            uint64_t index,
            uint64_t size,
            std::string_view subject);

        static void require_range(
            uint64_t offset,
            uint64_t count,
            uint64_t size,
            std::string_view subject,
            uint64_t invalid_offset = StaticScene::INVALID_INDEX);
    };

} // namespace fjr::scene
