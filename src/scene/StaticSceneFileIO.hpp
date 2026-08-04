#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string_view>
#include <vector>

namespace fjr::scene::static_scene_file_io {

    class Reader final {
    public:
        Reader(
            std::istream& source,
            std::uint64_t size,
            std::filesystem::path path);

        template<typename T>
        void read(T& value) {
            read_bytes(&value, sizeof(T));
        }

        template<typename T>
        void read(std::vector<T>& value) {
            std::size_t count = 0;
            read(count);
            if (count > remaining_ / sizeof(T)) {
                fail("Binary vector exceeds the input.");
            }

            try {
                value.resize(count);
            }
            catch (...) {
                fail("Failed to allocate a binary vector.");
            }
            read_bytes(value.data(), count * sizeof(T));
        }

        void skip(std::uint64_t size);
        void require_end();

        [[nodiscard]]
        std::uint64_t offset() const noexcept;

        [[nodiscard]]
        std::uint64_t remaining() const noexcept;

    private:
        void read_bytes(void* destination, std::size_t size);

        [[noreturn]]
        void fail(std::string_view message) const;

    private:
        std::istream& source_;
        std::uint64_t size_ = 0;
        std::uint64_t remaining_ = 0;
        std::filesystem::path path_;
    };

    class Writer final {
    public:
        Writer(
            std::ostream& destination,
            std::filesystem::path path);

        template<typename T>
        void write(const T& value) {
            write_bytes(&value, sizeof(T));
        }

        template<typename T>
        void write(const std::vector<T>& value) {
            const std::size_t count = value.size();
            write(count);
            write_bytes(value.data(), count * sizeof(T));
        }

        void copy(
            std::istream& source,
            std::uint64_t size,
            const std::filesystem::path& source_path);

        [[nodiscard]]
        std::uint64_t offset() const noexcept;

    private:
        void write_bytes(const void* source, std::size_t size);

        [[noreturn]]
        void fail(std::string_view message) const;

    private:
        std::ostream& destination_;
        std::filesystem::path path_;
        std::uint64_t offset_ = 0;
    };

    [[nodiscard]]
    std::uint64_t header_size() noexcept;

    void read_header(Reader& reader);

    void write_header(
        Writer& writer,
        std::uint64_t payload_size);

} // namespace fjr::scene::static_scene_file_io
