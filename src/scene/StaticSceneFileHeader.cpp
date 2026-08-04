#include "StaticSceneFileHeader.hpp"

#include "FastJungle/core/util/BinaryStream.hpp"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/scene/StaticScene.hpp"

#include <array>
#include <cstdint>

namespace fjr::scene::static_scene_file_header {

    namespace {

        constexpr std::array<char, 8> MAGIC{
            'F', 'J', 'S', 'C', 'E', 'N', 'E', '\0'
        };
        constexpr std::uint32_t VERSION = 1;

        struct Header final {
            std::array<char, 8> magic = MAGIC;
            std::uint32_t version = VERSION;
            std::uint32_t header_size = 32;
            std::uint32_t vertex_size = sizeof(StaticScene::Vertex);
            std::uint32_t scene_info_size = sizeof(StaticScene::SceneInfo);
            std::uint64_t payload_size = 0;
        };

        static_assert(sizeof(Header) == 32);

        void validate(
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

    } // namespace

    std::uint64_t size() noexcept {
        return sizeof(Header);
    }

    void read(util::BinaryReader& reader) {
        Header header;
        reader.read(header);
        validate(header, reader.remaining());
    }

    void write(
        util::BinaryWriter& writer,
        std::uint64_t payload_size) {

        Header header;
        header.payload_size = payload_size;
        writer.write(header);
    }

} // namespace fjr::scene::static_scene_file_header
