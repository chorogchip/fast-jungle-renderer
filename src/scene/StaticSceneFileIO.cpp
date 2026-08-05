#include "FastJungle/scene/StaticSceneFileIO.hpp"

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/scene/StaticScene.hpp"

#include <algorithm>
#include <array>
#include <istream>
#include <limits>
#include <ostream>
#include <utility>

namespace fjr::scene::static_scene_file_io {

    namespace {

        constexpr std::size_t BUFFER_SIZE = 4 * 1024 * 1024;
        constexpr std::array<char, 8> SCENE_MAGIC{
            'F', 'J', 'S', 'C', 'E', 'N', 'E', '\0'
        };
        constexpr std::uint32_t SCENE_VERSION = 5;
        constexpr std::array<char, 8> TEXTURE_MAGIC{
            'F', 'J', 'T', 'E', 'X', '\0', '\0', '\0'
        };
        constexpr std::uint32_t TEXTURE_VERSION = 1;

        struct SceneHeader final {
            std::array<char, 8> magic = SCENE_MAGIC;
            std::uint32_t version = SCENE_VERSION;
            std::uint32_t header_size = 40;
            std::uint32_t vertex_size = sizeof(StaticScene::Vertex);
            std::uint32_t scene_info_size = sizeof(StaticScene::SceneInfo);
            std::uint64_t payload_size = 0;
            std::uint64_t texture_payload_size = 0;
        };

        struct TextureHeader final {
            std::array<char, 8> magic = TEXTURE_MAGIC;
            std::uint32_t version = TEXTURE_VERSION;
            std::uint32_t header_size = 24;
            std::uint64_t payload_size = 0;
        };

        static_assert(sizeof(SceneHeader) == 40);
        static_assert(sizeof(TextureHeader) == 24);

        void validate_scene_header(
            const SceneHeader& header,
            std::uint64_t payload_size) {

            if (header.magic != SCENE_MAGIC) {
                log::Logger::g_logger
                    << "StaticScene file magic is invalid.\n";
                log::Logger::g_logger.abort();
            }
            if (header.version != SCENE_VERSION) {
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
            std::uint64_t payload_size) {

            if (header.magic != TEXTURE_MAGIC) {
                log::Logger::g_logger
                    << "StaticTexture file magic is invalid.\n";
                log::Logger::g_logger.abort();
            }
            if (header.version != TEXTURE_VERSION) {
                log::Logger::g_logger
                    << "StaticTexture file version is unsupported: "
                    << header.version << '\n';
                log::Logger::g_logger.abort();
            }
            if (header.header_size != sizeof(TextureHeader) ||
                header.payload_size != payload_size) {
                log::Logger::g_logger
                    << "StaticTexture file length is invalid.\n";
                log::Logger::g_logger.abort();
            }
        }

    } // namespace

    Reader::Reader(
        std::istream& source,
        std::uint64_t size,
        std::filesystem::path path)
        : source_(source),
          size_(size),
          remaining_(size),
          path_(std::move(path)) {}

    void Reader::skip(std::uint64_t size) {
        if (size > remaining_ ||
            size > static_cast<std::uint64_t>(
                std::numeric_limits<std::streamoff>::max())) {
            fail("Binary skip exceeds the input.");
        }

        source_.seekg(static_cast<std::streamoff>(size), std::ios::cur);
        if (!source_) {
            fail("Failed to seek the binary input.");
        }
        remaining_ -= size;
    }

    void Reader::require_end() {
        if (remaining_ != 0) {
            fail("Binary input has trailing bytes.");
        }
    }

    std::uint64_t Reader::offset() const noexcept {
        return size_ - remaining_;
    }

    std::uint64_t Reader::remaining() const noexcept {
        return remaining_;
    }

    void Reader::read_bytes(void* destination, std::size_t size) {
        if (size > remaining_) {
            fail("Binary input is truncated.");
        }

        auto* cursor = static_cast<char*>(destination);
        std::size_t left = size;
        while (left != 0) {
            const std::size_t chunk = std::min(left, BUFFER_SIZE);
            source_.read(cursor, static_cast<std::streamsize>(chunk));
            if (source_.gcount() != static_cast<std::streamsize>(chunk)) {
                fail("Failed to read the binary input.");
            }
            cursor += chunk;
            left -= chunk;
            remaining_ -= chunk;
        }
    }

    void Reader::fail(std::string_view message) const {
        log::Logger::g_logger
            << message << '\n'
            << "  path: " << path_ << '\n'
            << "  offset: " << offset() << '\n';
        log::Logger::g_logger.abort();
    }

    Writer::Writer(
        std::ostream& destination,
        std::filesystem::path path)
        : destination_(destination),
          path_(std::move(path)) {}

    void Writer::write_bytes(const void* source, std::size_t size) {
        const auto* cursor = static_cast<const char*>(source);
        while (size != 0) {
            const std::size_t chunk = std::min(size, BUFFER_SIZE);
            destination_.write(cursor, static_cast<std::streamsize>(chunk));
            if (!destination_) {
                fail("Failed to write the binary output.");
            }
            cursor += chunk;
            size -= chunk;
            offset_ += chunk;
        }
    }

    void Writer::copy(
        std::istream& source,
        std::uint64_t size,
        const std::filesystem::path& source_path) {

        std::vector<std::byte> buffer;
        try {
            buffer.resize(BUFFER_SIZE);
        }
        catch (...) {
            fail("Failed to allocate the binary copy buffer.");
        }

        while (size != 0) {
            const std::size_t chunk = static_cast<std::size_t>(
                std::min<std::uint64_t>(size, buffer.size()));
            source.read(
                reinterpret_cast<char*>(buffer.data()),
                static_cast<std::streamsize>(chunk));
            if (source.gcount() != static_cast<std::streamsize>(chunk)) {
                log::Logger::g_logger
                    << "Binary copy input is truncated: "
                    << source_path << '\n';
                log::Logger::g_logger.abort();
            }
            write_bytes(buffer.data(), chunk);
            size -= chunk;
        }

        if (source.peek() != std::char_traits<char>::eof()) {
            log::Logger::g_logger
                << "Binary copy input has trailing bytes: "
                << source_path << '\n';
            log::Logger::g_logger.abort();
        }
    }

    std::uint64_t Writer::offset() const noexcept {
        return offset_;
    }

    void Writer::fail(std::string_view message) const {
        log::Logger::g_logger
            << message << '\n'
            << "  path: " << path_ << '\n'
            << "  offset: " << offset_ << '\n';
        log::Logger::g_logger.abort();
    }

    std::uint64_t header_size() noexcept {
        return sizeof(SceneHeader);
    }

    std::uint64_t read_header(Reader& reader) {
        SceneHeader header;
        reader.read(header);
        validate_scene_header(header, reader.remaining());
        return header.texture_payload_size;
    }

    void write_header(
        Writer& writer,
        std::uint64_t payload_size,
        std::uint64_t texture_payload_size) {

        SceneHeader header;
        header.payload_size = payload_size;
        header.texture_payload_size = texture_payload_size;
        writer.write(header);
    }

    std::uint64_t texture_header_size() noexcept {
        return sizeof(TextureHeader);
    }

    std::uint64_t read_texture_header(Reader& reader) {
        TextureHeader header;
        reader.read(header);
        validate_texture_header(header, reader.remaining());
        return header.payload_size;
    }

    void write_texture_header(
        Writer& writer,
        std::uint64_t payload_size) {

        TextureHeader header;
        header.payload_size = payload_size;
        writer.write(header);
    }

} // namespace fjr::scene::static_scene_file_io
