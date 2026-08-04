#pragma once

#include <cstdint>

namespace fjr::util {
    class BinaryReader;
    class BinaryWriter;
}

namespace fjr::scene::static_scene_file_header {

    [[nodiscard]]
    std::uint64_t size() noexcept;

    void read(util::BinaryReader& reader);

    void write(
        util::BinaryWriter& writer,
        std::uint64_t payload_size);

} // namespace fjr::scene::static_scene_file_header
