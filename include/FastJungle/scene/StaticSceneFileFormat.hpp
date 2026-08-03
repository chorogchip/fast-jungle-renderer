#pragma once

#include <array>
#include <cstdint>

#include "FastJungle/scene/StaticScene.hpp"

namespace fjr::util {
    class BinaryReader;
    class BinaryWriter;
}

namespace fjr::scene {

    class StaticSceneFileFormat final {
    private:
        static constexpr std::array<char, 8> MAGIC{
            'F', 'J', 'S', 'C', 'E', 'N', 'E', '\0'
        };
        static constexpr std::uint32_t VERSION = 1;

        struct Header final {
            std::array<char, 8> magic = MAGIC;
            std::uint32_t version = VERSION;
            std::uint32_t header_size = 32;
            std::uint32_t vertex_size = sizeof(StaticScene::Vertex);
            std::uint32_t scene_info_size = sizeof(StaticScene::SceneInfo);
            std::uint64_t payload_size = 0;
        };

        static_assert(sizeof(Header) == 32);

    public:
        StaticSceneFileFormat() = delete;

        [[nodiscard]]
        static constexpr std::uint64_t header_size() {
            return sizeof(Header);
        }

        static void read_header(util::BinaryReader& reader);

        static void write_header(
            util::BinaryWriter& writer,
            std::uint64_t payload_size);

    private:
        static void validate(
            const Header& header,
            std::uint64_t payload_size);
    };

} // namespace fjr::scene
