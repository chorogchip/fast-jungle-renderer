#include "FastJungle/scene/StaticSceneFileIO.hpp"

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/scene/StaticScene.hpp"

#include <array>

namespace fjr::scene::static_scene_file_io {

    namespace {

        constexpr std::array<char, 8> SCENE_MAGIC{
            'F', 'J', 'S', 'C', 'E', 'N', 'E', '\0'
        };
        constexpr std::array<char, 8> TEXTURE_MAGIC{
            'F', 'J', 'T', 'E', 'X', '\0', '\0', '\0'
        };

        struct SceneHeader final {
            std::array<char, 8> magic = SCENE_MAGIC;
            uint32_t version = SCENE_FORMAT_VERSION;
            uint32_t header_size = 40;
            uint32_t vertex_size = sizeof(StaticScene::Vertex);
            uint32_t scene_info_size = sizeof(StaticScene::SceneInfo);
            uint64_t payload_size = 0;
            uint64_t texture_payload_size = 0;
        };

        struct TextureHeader final {
            std::array<char, 8> magic = TEXTURE_MAGIC;
            uint32_t version = TEXTURE_FORMAT_VERSION;
            uint32_t header_size = 32;
            uint64_t metadata_size = 0;
            uint64_t payload_size = 0;
        };

        static_assert(sizeof(SceneHeader) == 40);
        static_assert(sizeof(TextureHeader) == 32);

        void validate_scene_header(
            const SceneHeader& header,
            uint64_t payload_size) {

            if (header.magic != SCENE_MAGIC) {
                log::Logger::g_logger
                    << "StaticScene file magic is invalid.\n";
                log::Logger::g_logger.abort();
            }
            if (header.version != SCENE_FORMAT_VERSION) {
                log::Logger::g_logger
                    << "StaticScene file version is unsupported: "
                    << header.version << '\n';
                log::Logger::g_logger.abort();
            }
            if (header.header_size != sizeof(SceneHeader) ||
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

        void validate_texture_header(
            const TextureHeader& header,
            uint64_t file_size) {

            if (header.magic != TEXTURE_MAGIC) {
                log::Logger::g_logger
                    << "StaticTexture file magic is invalid.\n";
                log::Logger::g_logger.abort();
            }
            if (header.version != TEXTURE_FORMAT_VERSION) {
                log::Logger::g_logger
                    << "StaticTexture file version is unsupported: "
                    << header.version << '\n';
                log::Logger::g_logger.abort();
            }
            if (header.header_size != sizeof(TextureHeader) ||
                header.metadata_size > file_size ||
                header.payload_size != file_size - header.metadata_size) {
                log::Logger::g_logger
                    << "StaticTexture file length is invalid.\n";
                log::Logger::g_logger.abort();
            }
        }

    } // namespace

    uint64_t header_size() noexcept {
        return sizeof(SceneHeader);
    }

    uint64_t read_header(util::BinaryReader& reader) {
        SceneHeader header;
        reader.read(header);
        validate_scene_header(header, reader.remaining());
        return header.texture_payload_size;
    }

    void write_header(
        util::BinaryWriter& writer,
        uint64_t payload_size,
        uint64_t texture_payload_size) {

        SceneHeader header;
        header.payload_size = payload_size;
        header.texture_payload_size = texture_payload_size;
        writer.write(header);
    }

    uint64_t texture_header_size() noexcept {
        return sizeof(TextureHeader);
    }

    TextureHeaderInfo read_texture_header(util::BinaryReader& reader) {
        TextureHeader header;
        reader.read(header);
        validate_texture_header(header, reader.remaining());
        return {
            .metadata_size = header.metadata_size,
            .payload_size = header.payload_size,
        };
    }

    void write_texture_header(
        util::BinaryWriter& writer,
        uint64_t metadata_size,
        uint64_t payload_size) {

        TextureHeader header;
        header.metadata_size = metadata_size;
        header.payload_size = payload_size;
        writer.write(header);
    }

} // namespace fjr::scene::static_scene_file_io
