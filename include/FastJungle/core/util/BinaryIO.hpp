#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string_view>
#include <vector>

namespace fjr::util {

    class BinarySize final {
    public:
        void add(uint64_t size);
        void add_vector(uint64_t count, uint64_t element_size);

        [[nodiscard]] uint64_t value() const noexcept;

    private:
        uint64_t value_ = 0;
    };

    class BinaryReader final {
    public:
        BinaryReader(
            std::istream& source,
            uint64_t size,
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

        void read_raw(void* destination, std::size_t size);
        void skip(uint64_t size);
        void require_end();

        [[nodiscard]] uint64_t offset() const noexcept;
        [[nodiscard]] uint64_t remaining() const noexcept;

    private:
        void read_bytes(void* destination, std::size_t size);
        [[noreturn]] void fail(std::string_view message) const;

        std::istream& source_;
        uint64_t size_ = 0;
        uint64_t remaining_ = 0;
        std::filesystem::path path_;
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

        void write_raw(const void* source, std::size_t size);
        void copy(
            std::istream& source,
            uint64_t size,
            const std::filesystem::path& source_path);

        [[nodiscard]] uint64_t offset() const noexcept;

    private:
        void write_bytes(const void* source, std::size_t size);
        [[noreturn]] void fail(std::string_view message) const;

        std::ostream& destination_;
        std::filesystem::path path_;
        uint64_t offset_ = 0;
    };

} // namespace fjr::util
