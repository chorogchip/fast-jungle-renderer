#include "FastJungle/scene/StaticTextureFileFormat.hpp"

#include "FastJungle/core/util/BinaryStream.hpp"
#include "FastJungle/core/util/Logger.hpp"

namespace fjr::scene {

    std::uint64_t StaticTextureFileFormat::read_header(
        util::BinaryReader& reader) {

        Header header;
        reader.read(header);
        if (header.magic != MAGIC) {
            log::Logger::g_logger
                << "StaticTexture file magic is invalid.\n";
            log::Logger::g_logger.abort();
        }
        if (header.version != VERSION) {
            log::Logger::g_logger
                << "StaticTexture file version is unsupported: "
                << header.version << '\n';
            log::Logger::g_logger.abort();
        }
        if (header.header_size != sizeof(Header) ||
            header.payload_size != reader.remaining()) {
            log::Logger::g_logger
                << "StaticTexture file length is invalid.\n";
            log::Logger::g_logger.abort();
        }
        return header.payload_size;
    }

    void StaticTextureFileFormat::write_header(
        util::BinaryWriter& writer,
        std::uint64_t payload_size) {

        Header header;
        header.payload_size = payload_size;
        writer.write(header);
    }

} // namespace fjr::scene
