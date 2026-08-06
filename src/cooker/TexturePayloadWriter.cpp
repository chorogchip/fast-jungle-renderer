#include "FastJungle/cooker/TexturePayloadWriter.hpp"

#include <DirectXTex.h>

#include <cstddef>
#include <limits>
#include <system_error>
#include <utility>

#include "FastJungle/cooker/CookerCommon.hpp"

namespace fjr::cooker {
    namespace {
        void write_bytes(
            std::ofstream& output,
            const std::byte* data,
            std::size_t size,
            const std::filesystem::path& path) {
            if (size > static_cast<std::size_t>(
                std::numeric_limits<std::streamsize>::max())) {
                fail("Texture mip is too large: ", path.generic_string());
            }
            output.write(
                reinterpret_cast<const char*>(data),
                static_cast<std::streamsize>(size));
            if (!output) {
                fail(
                    "Failed to write texture payload: ",
                    path.generic_string());
            }
        }
    } // namespace

    TexturePayloadWriter::TexturePayloadWriter(std::filesystem::path path)
        : path_{std::move(path)}
        , output_{path_, std::ios::binary | std::ios::trunc} {
        if (!output_.is_open()) {
            fail("Failed to open texture payload: ", path_.generic_string());
        }
    }

    void TexturePayloadWriter::append(
        const DirectX::ScratchImage& image,
        scene::StaticScene::Texture& texture,
        std::vector<scene::StaticScene::TextureMip>& mips,
        const std::filesystem::path& source_path) {
        const auto metadata = image.GetMetadata();
        texture.width = checked_u32(metadata.width, "Texture width");
        texture.height = checked_u32(metadata.height, "Texture height");
        texture.dxgi_format = static_cast<std::uint32_t>(metadata.format);
        texture.mip_offset = checked_u32(mips.size(), "Texture mip offset");
        texture.mip_count = checked_u32(metadata.mipLevels, "Texture mip count");
        texture.data_byte_offset = size_;

        for (std::size_t mip = 0; mip < metadata.mipLevels; ++mip) {
            const auto* source = image.GetImage(mip, 0, 0);
            if (source == nullptr || source->pixels == nullptr) {
                fail("Texture mip is missing: ", source_path.generic_string());
            }

            scene::StaticScene::TextureMip destination;
            destination.width = checked_u32(source->width, "Texture mip width");
            destination.height = checked_u32(
                source->height,
                "Texture mip height");
            destination.row_pitch = checked_u32(
                source->rowPitch,
                "Texture mip row pitch");
            destination.slice_pitch = checked_u32(
                source->slicePitch,
                "Texture mip slice pitch");
            destination.data_byte_offset_local =
                size_ - texture.data_byte_offset;
            mips.push_back(destination);

            if (source->slicePitch >
                std::numeric_limits<std::uint64_t>::max() - size_) {
                fail("Texture payload exceeds uint64_t.");
            }
            write_bytes(
                output_,
                reinterpret_cast<const std::byte*>(source->pixels),
                source->slicePitch,
                source_path);
            size_ += source->slicePitch;
        }

        texture.data_size = size_ - texture.data_byte_offset;
    }

    std::uint64_t TexturePayloadWriter::finish() {
        output_.flush();
        if (!output_) {
            fail("Failed to flush texture payload: ", path_.generic_string());
        }
        output_.close();
        if (!output_) {
            fail("Failed to close texture payload: ", path_.generic_string());
        }
        return size_;
    }

    void TexturePayloadWriter::abandon() noexcept {
        output_.close();
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

} // namespace fjr::cooker
