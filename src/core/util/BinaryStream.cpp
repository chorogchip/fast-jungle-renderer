#include "FastJungle/core/util/BinaryStream.hpp"

#include "FastJungle/core/util/Logger.hpp"

#include <algorithm>
#include <cstring>
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
          path_(std::move(path)) {

        try {
            buffer_.resize(BUFFER_SIZE);
        }
        catch (...) {
            fail("Failed to allocate the binary read buffer.");
        }
    }

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

    void BinaryReader::require_stream(
        std::istream& expected,
        std::uint64_t size,
        const std::filesystem::path& expected_path,
        std::string_view name) {

        std::vector<std::byte> expected_buffer;
        try {
            expected_buffer.resize(BUFFER_SIZE);
        }
        catch (...) {
            fail("Failed to allocate the binary compare buffer.");
        }

        while (size != 0) {
            const std::size_t chunk = static_cast<std::size_t>(
                std::min<std::uint64_t>(size, buffer_.size()));
            read_bytes(buffer_.data(), chunk);
            expected.read(
                reinterpret_cast<char*>(expected_buffer.data()),
                static_cast<std::streamsize>(chunk));
            if (expected.gcount() != static_cast<std::streamsize>(chunk)) {
                log::Logger::g_logger
                    << "Binary compare input is truncated: "
                    << expected_path << '\n';
                log::Logger::g_logger.abort();
            }
            if (std::memcmp(
                buffer_.data(),
                expected_buffer.data(),
                chunk) != 0) {
                fail_changed(name);
            }
            size -= chunk;
        }

        if (expected.peek() != std::char_traits<char>::eof()) {
            log::Logger::g_logger
                << "Binary compare input has trailing bytes: "
                << expected_path << '\n';
            log::Logger::g_logger.abort();
        }
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

    void BinaryReader::require_bytes(
        const void* expected,
        std::size_t size,
        std::string_view name) {

        const auto* cursor = static_cast<const std::byte*>(expected);
        while (size != 0) {
            const std::size_t chunk = std::min(size, buffer_.size());
            read_bytes(buffer_.data(), chunk);
            if (std::memcmp(buffer_.data(), cursor, chunk) != 0) {
                fail_changed(name);
            }
            cursor += chunk;
            size -= chunk;
        }
    }

    void BinaryReader::fail(std::string_view message) const {
        log::Logger::g_logger
            << message << '\n'
            << "  path: " << path_ << '\n'
            << "  offset: " << offset() << '\n';
        log::Logger::g_logger.abort();
    }

    void BinaryReader::fail_changed(std::string_view name) const {
        log::Logger::g_logger
            << "Binary data changed: " << name << '\n'
            << "  path: " << path_ << '\n'
            << "  offset: " << offset() << '\n';
        log::Logger::g_logger.abort();
    }

    BinaryWriter::BinaryWriter(
        std::ostream& destination,
        std::filesystem::path path)
        : destination_(destination),
          path_(std::move(path)) {

        try {
            buffer_.resize(BUFFER_SIZE);
        }
        catch (...) {
            fail("Failed to allocate the binary write buffer.");
        }
    }

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

        while (size != 0) {
            const std::size_t chunk = static_cast<std::size_t>(
                std::min<std::uint64_t>(size, buffer_.size()));
            source.read(
                reinterpret_cast<char*>(buffer_.data()),
                static_cast<std::streamsize>(chunk));
            if (source.gcount() != static_cast<std::streamsize>(chunk)) {
                log::Logger::g_logger
                    << "Binary copy input is truncated: "
                    << source_path << '\n';
                log::Logger::g_logger.abort();
            }
            write_bytes(buffer_.data(), chunk);
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
