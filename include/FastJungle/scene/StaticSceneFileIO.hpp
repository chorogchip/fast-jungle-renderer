#pragma once

#include <cstdint>

#include "FastJungle/core/util/BinaryIO.hpp"

namespace fjr::scene::static_scene_file_io {

    // v18: Added cooked software-raster clusters.
    inline constexpr uint32_t SCENE_FORMAT_VERSION = 18;
    // v7: alpha-tested materials use dedicated RGB/BC1 and coverage-preserved
    // opacity/BC4 payloads. Impostors preserve signed normals in linear BC7.
    inline constexpr uint32_t TEXTURE_FORMAT_VERSION = 7;

    [[nodiscard]]
    uint64_t header_size() noexcept;

    [[nodiscard]]
    uint64_t read_header(util::BinaryReader& reader);

    void write_header(
        util::BinaryWriter& writer,
        uint64_t payload_size,
        uint64_t texture_payload_size);

    [[nodiscard]]
    uint64_t texture_header_size() noexcept;

    struct TextureHeaderInfo final {
        uint64_t metadata_size = 0;
        uint64_t payload_size = 0;
    };

    [[nodiscard]]
    TextureHeaderInfo read_texture_header(util::BinaryReader& reader);

    void write_texture_header(
        util::BinaryWriter& writer,
        uint64_t metadata_size,
        uint64_t payload_size);

} // namespace fjr::scene::static_scene_file_io
