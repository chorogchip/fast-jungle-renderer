#include "TexturePayloadBuilder.hpp"

#include <DirectXTex.h>

#include <cstddef>
#include <limits>
#include <utility>

#include "FastJungle/core/util/File.hpp"
#include "FastJungle/core/util/Logger.hpp"

namespace fjr::cooker {
    namespace {
        void write_bytes(
            std::ofstream& output,
            const std::byte* data,
            std::size_t size,
            const std::filesystem::path& path) {
            if (size > static_cast<std::size_t>(
                std::numeric_limits<std::streamsize>::max())) {
                log::Logger::g_logger << log::abrt(
                    "Texture mip is too large: " + path.generic_string());
            }
            output.write(
                reinterpret_cast<const char*>(data),
                static_cast<std::streamsize>(size));
            if (!output) {
                log::Logger::g_logger << log::abrt(
                    "Failed to write texture payload: " + path.generic_string());
            }
        }
    } // namespace

    TexturePayloadBuilder::TexturePayloadBuilder(std::filesystem::path path)
        : file_{std::move(path)},
          output_{util::File::open_write(file_.path())} {}

    void TexturePayloadBuilder::append(
        const DirectX::ScratchImage& image,
        scene::StaticScene::Texture& texture,
        std::vector<scene::StaticScene::TextureMip>& mips,
        const std::filesystem::path& source_path) {
        const auto metadata = image.GetMetadata();
        texture.width = static_cast<uint32_t>(metadata.width);
        texture.height = static_cast<uint32_t>(metadata.height);
        texture.dxgi_format = static_cast<uint32_t>(metadata.format);
        texture.mip_offset = static_cast<uint32_t>(mips.size());
        texture.mip_count = static_cast<uint32_t>(metadata.mipLevels);
        texture.data_byte_offset = size_;

        for (std::size_t mip = 0; mip < metadata.mipLevels; ++mip) {
            const auto* source = image.GetImage(mip, 0, 0);
            if (source == nullptr || source->pixels == nullptr) {
                log::Logger::g_logger << log::abrt(
                    "Texture mip is missing: " + source_path.generic_string());
            }

            scene::StaticScene::TextureMip destination;
            destination.width = static_cast<uint32_t>(source->width);
            destination.height = static_cast<uint32_t>(source->height);
            destination.row_pitch = static_cast<uint32_t>(source->rowPitch);
            destination.slice_pitch = static_cast<uint32_t>(source->slicePitch);
            destination.data_byte_offset_local =
                size_ - texture.data_byte_offset;
            mips.push_back(destination);

            if (source->slicePitch >
                std::numeric_limits<uint64_t>::max() - size_) {
                log::Logger::g_logger << log::abrt(
                    "Texture payload exceeds uint64_t.");
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

    scene::StaticTexturePayload TexturePayloadBuilder::finish() {
        util::File::finish(output_, file_.path());
        return {std::move(file_), size_};
    }

} // namespace fjr::cooker
