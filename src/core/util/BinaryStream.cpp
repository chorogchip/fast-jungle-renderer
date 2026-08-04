#include "FastJungle/core/util/BinaryStream.hpp"

#include "FastJungle/core/util/Logger.hpp"

#include <algorithm>
#include <istream>
#include <limits>
#include <ostream>
#include <utility>

namespace fjr::util {

    namespace {

        constexpr std::size_t BUFFER_SIZE = 4 * 1024 * 1024;

    } // namespace

    BinaryReader::BinaryReader(
        std::istream& source,
        std::uint64_t size,
        std::filesystem::path path)
        : source_(source),
          size_(size),
          remaining_(size),
          path_(std::move(path)) {}

    void BinaryReader::skip(std::uint64_t size) {
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

    void BinaryReader::require_end() {
        if (remaining_ != 0) {
            fail("Binary input has trailing bytes.");
        }
    }

    std::uint64_t BinaryReader::offset() const noexcept {
        return size_ - remaining_;
    }

    std::uint64_t BinaryReader::remaining() const noexcept {
        return remaining_;
    }

    void BinaryReader::read_bytes(void* destination, std::size_t size) {
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

    void BinaryReader::fail(std::string_view message) const {
        log::Logger::g_logger
            << message << '\n'
            << "  path: " << path_ << '\n'
            << "  offset: " << offset() << '\n';
        log::Logger::g_logger.abort();
    }

    BinaryWriter::BinaryWriter(
        std::ostream& destination,
        std::filesystem::path path)
        : destination_(destination),
          path_(std::move(path)) {}

    void BinaryWriter::write_bytes(const void* source, std::size_t size) {
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

    void BinaryWriter::copy(
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

    std::uint64_t BinaryWriter::offset() const noexcept {
        return offset_;
    }

    void BinaryWriter::fail(std::string_view message) const {
        log::Logger::g_logger
            << message << '\n'
            << "  path: " << path_ << '\n'
            << "  offset: " << offset_ << '\n';
        log::Logger::g_logger.abort();
    }

} // namespace fjr::util
