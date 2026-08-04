#include "FastJungle/scene/StaticSceneFileFormat.hpp"

#include "FastJungle/core/util/BinaryStream.hpp"
#include "FastJungle/core/util/Logger.hpp"

namespace fjr::scene {

    std::uint64_t StaticSceneFileFormat::read_header(
        util::BinaryReader& reader) {

        Header header;
        reader.read(header);
        validate(header, reader.remaining());
        return header.texture_payload_size;
    }

    void StaticSceneFileFormat::write_header(
        util::BinaryWriter& writer,
        std::uint64_t payload_size,
        std::uint64_t texture_payload_size) {

        Header header;
        header.payload_size = payload_size;
        header.texture_payload_size = texture_payload_size;
        writer.write(header);
    }

    void StaticSceneFileFormat::validate(
        const Header& header,
        std::uint64_t payload_size) {

        if (header.magic != MAGIC) {
            log::Logger::g_logger
                << "StaticScene file magic is invalid.\n";
            log::Logger::g_logger.abort();
        }
        if (header.version != VERSION) {
            log::Logger::g_logger
                << "StaticScene file version is unsupported: "
                << header.version << '\n';
            log::Logger::g_logger.abort();
        }
        if (header.header_size != sizeof(Header) ||
            header.vertex_size != sizeof(StaticScene::Vertex) ||
            header.scene_info_size != sizeof(StaticScene::SceneInfo)) {
            log::Logger::g_logger
                << "StaticScene file ABI does not match this build.\n";
            log::Logger::g_logger.abort();
        }
        if (header.payload_size != payload_size) {
            log::Logger::g_logger
                << "StaticScene file length is invalid.\n";
            log::Logger::g_logger.abort();
        }
    }

} // namespace fjr::scene
