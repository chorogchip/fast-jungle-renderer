#pragma once

#include <cstdint>
#include <filesystem>

#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::scene {

    class StaticSceneWriter final {
    public:
        StaticSceneWriter() = delete;

        static void save(
            const std::filesystem::path& path,
            const StaticScene& scene);

        static void save(
            const std::filesystem::path& path,
            const StaticScene& scene,
            const std::filesystem::path& texture_payload_path,
            std::uint64_t texture_payload_size);

        [[nodiscard]]
        static std::uint64_t calculate_size(
            const StaticScene& scene,
            std::uint64_t texture_payload_size);

    private:
        static void add(
            std::uint64_t& total,
            std::uint64_t size);

        static void add_vector(
            std::uint64_t& total,
            std::uint64_t count,
            std::uint64_t element_size);
    };

} // namespace fjr::scene
