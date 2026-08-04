#pragma once

#include <array>
#include <cstdint>

namespace fjr::util {
    class BinaryReader;
    class BinaryWriter;
}

namespace fjr::scene {

    class StaticTextureFileFormat final {
    private:
        static constexpr std::array<char, 8> MAGIC{
            'F', 'J', 'T', 'E', 'X', '\0', '\0', '\0'
        };
        static constexpr std::uint32_t VERSION = 1;

        struct Header final {
            std::array<char, 8> magic = MAGIC;
            std::uint32_t version = VERSION;
            std::uint32_t header_size = 24;
            std::uint64_t payload_size = 0;
        };

        static_assert(sizeof(Header) == 24);

    public:
        StaticTextureFileFormat() = delete;

        [[nodiscard]]
        static constexpr std::uint64_t header_size() noexcept {
            return sizeof(Header);
        }

        [[nodiscard]]
        static std::uint64_t read_header(util::BinaryReader& reader);

        static void write_header(
            util::BinaryWriter& writer,
            std::uint64_t payload_size);
    };

} // namespace fjr::scene
