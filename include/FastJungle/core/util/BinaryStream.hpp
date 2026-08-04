#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string_view>
#include <vector>

namespace fjr::util {

    class BinaryReader final {
    public:
        BinaryReader(
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

        template<typename T>
        void require_vector(
            const std::vector<T>& expected,
            std::string_view name) {

            std::size_t count = 0;
            read(count);
            if (count != expected.size()) {
                fail_changed(name);
            }
            require_bytes(expected.data(), count * sizeof(T), name);
        }

        template<typename T>
        void require_record(const T& expected, std::string_view name) {
            require_bytes(&expected, sizeof(T), name);
        }

        void skip(std::uint64_t size);
        void read_raw(void* destination, std::size_t size) {
            read_bytes(destination, size);
        }
        void require_stream(
            std::istream& expected,
            std::uint64_t size,
            const std::filesystem::path& expected_path,
            std::string_view name);
        void require_end();

        [[nodiscard]]
        std::uint64_t offset() const noexcept;

        [[nodiscard]]
        std::uint64_t remaining() const noexcept;

    private:
        void read_bytes(void* destination, std::size_t size);
        void require_bytes(
            const void* expected,
            std::size_t size,
            std::string_view name);

        [[noreturn]]
        void fail(std::string_view message) const;

        [[noreturn]]
        void fail_changed(std::string_view name) const;

    private:
        std::istream& source_;
        std::uint64_t size_ = 0;
        std::uint64_t remaining_ = 0;
        std::filesystem::path path_;
        std::vector<std::byte> buffer_;
    };

    class BinaryWriter final {
    public:
        BinaryWriter(
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

        void write_bytes(const void* source, std::size_t size);
        void copy(
            std::istream& source,
            std::uint64_t size,
            const std::filesystem::path& source_path);

        [[nodiscard]]
        std::uint64_t offset() const noexcept;

    private:
        [[noreturn]]
        void fail(std::string_view message) const;

    private:
        std::ostream& destination_;
        std::filesystem::path path_;
        std::uint64_t offset_ = 0;
        std::vector<std::byte> buffer_;
    };

} // namespace fjr::util
