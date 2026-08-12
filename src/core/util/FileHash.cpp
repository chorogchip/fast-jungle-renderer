#include "FastJungle/core/util/FileHash.hpp"

#include "FastJungle/core/util/File.hpp"
#include "FastJungle/core/util/Logger.hpp"

#include <Windows.h>
#include <bcrypt.h>

#include <string_view>
#include <vector>

namespace fjr::util {

    Sha256 FileHash::sha256(const std::filesystem::path& path) {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        std::vector<uint8_t> hash_object;

        const auto close = [&algorithm, &hash]() noexcept {
            if (hash != nullptr) {
                BCryptDestroyHash(hash);
                hash = nullptr;
            }
            if (algorithm != nullptr) {
                BCryptCloseAlgorithmProvider(algorithm, 0);
                algorithm = nullptr;
            }
        };
        const auto require_success = [&close](
            NTSTATUS result,
            std::string_view operation) {
            if (!BCRYPT_SUCCESS(result)) {
                close();
                log::fail("SHA-256 operation failed: ", operation);
            }
        };

        require_success(
            BCryptOpenAlgorithmProvider(
                &algorithm,
                BCRYPT_SHA256_ALGORITHM,
                nullptr,
                0),
            "open");

        DWORD hash_object_size = 0;
        DWORD property_size = 0;
        require_success(
            BCryptGetProperty(
                algorithm,
                BCRYPT_OBJECT_LENGTH,
                reinterpret_cast<PUCHAR>(&hash_object_size),
                sizeof(hash_object_size),
                &property_size,
                0),
            "object size");
        if (property_size != sizeof(hash_object_size)) {
            close();
            log::fail("SHA-256 object size is invalid.");
        }

        hash_object.resize(hash_object_size);
        require_success(
            BCryptCreateHash(
                algorithm,
                &hash,
                hash_object.data(),
                hash_object_size,
                nullptr,
                0,
                0),
            "create");

        auto input = File::open_read(path);
        std::vector<uint8_t> buffer(1024 * 1024);
        while (input) {
            input.read(
                reinterpret_cast<char*>(buffer.data()),
                static_cast<std::streamsize>(buffer.size()));
            const auto size = input.gcount();
            if (size > 0) {
                require_success(
                    BCryptHashData(
                        hash,
                        buffer.data(),
                        static_cast<ULONG>(size),
                        0),
                    "update");
            }
        }
        if (!input.eof()) {
            close();
            log::fail("Failed to read file for SHA-256: ", path);
        }

        Sha256 result;
        require_success(
            BCryptFinishHash(
                hash,
                result.bytes.data(),
                static_cast<ULONG>(result.bytes.size()),
                0),
            "finish");
        close();
        return result;
    }

} // namespace fjr::util
