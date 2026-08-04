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
            std::uint64_t texture_payload_size);

    private:
        static void require_index(
            std::uint64_t index,
            std::uint64_t size,
            std::string_view subject);

        static void require_string(
            const StaticScene& scene,
            std::uint32_t offset,
            std::string_view subject);

        static void require_range(
            std::uint64_t offset,
            std::uint64_t count,
            std::uint64_t size,
            std::string_view subject,
            std::uint64_t invalid_offset = StaticScene::INVALID_INDEX);
    };

} // namespace fjr::scene
