#include "FastJungle/cooker/TextureCooker.hpp"

#include "CookerCommon.hpp"

#include <DirectXTex.h>

#if defined(FASTJUNGLE_HAS_OPENEXR)
#include <DirectXTexEXR.h>
#endif

#include <Windows.h>
#include <objbase.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "TextureImageProcessing.hpp"

namespace fjr::cooker {
    namespace {
        [[nodiscard]] std::string lowercase_extension(
            const std::filesystem::path& path) {
            auto result = path.extension().string();
            std::ranges::transform(
                result,
                result.begin(),
                [](unsigned char value) {
                    return static_cast<char>(std::tolower(value));
                });
            return result;
        }

        enum class TextureFileKind {
            DDS,
            TGA,
            HDR,
            EXR,
            WIC
        };

        [[nodiscard]] TextureFileKind texture_file_kind(
            const std::filesystem::path& path) {
            const auto extension = lowercase_extension(path);
            if (extension == ".dds") return TextureFileKind::DDS;
            if (extension == ".tga") return TextureFileKind::TGA;
            if (extension == ".hdr") return TextureFileKind::HDR;
            if (extension == ".exr") return TextureFileKind::EXR;
            return TextureFileKind::WIC;
        }

        class ComScope final {
        public:
            ComScope() {
                const HRESULT result = CoInitializeEx(
                    nullptr,
                    COINIT_MULTITHREADED);
                if (SUCCEEDED(result)) {
                    uninitialize_ = true;
                }
                else if (result != RPC_E_CHANGED_MODE) {
                    fail("CoInitializeEx failed.");
                }
            }

            ~ComScope() {
                if (uninitialize_) {
                    CoUninitialize();
                }
            }

            ComScope(const ComScope&) = delete;
            ComScope& operator=(const ComScope&) = delete;

        private:
            bool uninitialize_ = false;
        };

        void load_texture_metadata(
            const std::filesystem::path& path,
            DirectX::TexMetadata& metadata) {
            HRESULT result = E_FAIL;
            switch (texture_file_kind(path)) {
            case TextureFileKind::DDS:
                result = DirectX::GetMetadataFromDDSFile(
                    path.c_str(),
                    DirectX::DDS_FLAGS_NONE,
                    metadata);
                break;
            case TextureFileKind::TGA:
                result = DirectX::GetMetadataFromTGAFile(
                    path.c_str(),
                    metadata);
                break;
            case TextureFileKind::HDR:
                result = DirectX::GetMetadataFromHDRFile(
                    path.c_str(),
                    metadata);
                break;
            case TextureFileKind::EXR:
#if defined(FASTJUNGLE_HAS_OPENEXR)
                result = DirectX::GetMetadataFromEXRFile(
                    path.c_str(),
                    metadata);
#else
                fail(
                    "EXR texture support is disabled: ",
                    path.generic_string());
#endif
                break;
            case TextureFileKind::WIC:
                result = DirectX::GetMetadataFromWICFile(
                    path.c_str(),
                    DirectX::WIC_FLAGS_NONE,
                    metadata);
                break;
            }

            if (FAILED(result)) {
                fail(
                    "Texture metadata read failed: ",
                    path.generic_string());
            }
        }

        void load_texture(
            const std::filesystem::path& path,
            DirectX::TexMetadata& metadata,
            DirectX::ScratchImage& image) {
            HRESULT result = E_FAIL;
            switch (texture_file_kind(path)) {
            case TextureFileKind::DDS:
                result = DirectX::LoadFromDDSFile(
                    path.c_str(),
                    DirectX::DDS_FLAGS_NONE,
                    &metadata,
                    image);
                break;
            case TextureFileKind::TGA:
                result = DirectX::LoadFromTGAFile(
                    path.c_str(),
                    &metadata,
                    image);
                break;
            case TextureFileKind::HDR:
                result = DirectX::LoadFromHDRFile(
                    path.c_str(),
                    &metadata,
                    image);
                break;
            case TextureFileKind::EXR:
#if defined(FASTJUNGLE_HAS_OPENEXR)
                result = DirectX::LoadFromEXRFile(
                    path.c_str(),
                    &metadata,
                    image);
#else
                fail(
                    "EXR texture support is disabled: ",
                    path.generic_string());
#endif
                break;
            case TextureFileKind::WIC:
                result = DirectX::LoadFromWICFile(
                    path.c_str(),
                    DirectX::WIC_FLAGS_NONE,
                    &metadata,
                    image);
                break;
            }

            if (FAILED(result)) {
                fail("Texture decode failed: ", path.generic_string());
            }
        }

        void validate_texture_metadata(
            const DirectX::TexMetadata& metadata,
            const std::filesystem::path& path) {
            if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D ||
                metadata.arraySize != 1 ||
                metadata.depth != 1 ||
                metadata.mipLevels == 0) {
                fail(
                    "Texture is not a single 2D image: ",
                    path.generic_string());
            }
        }

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

    std::uint64_t TextureCooker::cook(
        scene::StaticScene& scene,
        std::span<const std::string> texture_paths,
        const std::filesystem::path& payload_path,
        TextureCookOptions options) {
        if (scene.textures.size() != texture_paths.size()) {
            throw std::invalid_argument(
                "Texture source count does not match StaticScene textures.");
        }
        if (!scene.texture_data.empty() || !scene.texture_mips.empty()) {
            throw std::invalid_argument(
                "TextureCooker requires an uncooked StaticScene.");
        }

        const ComScope com_scope;
        const auto compression_plans =
            resolve_texture_compression(scene);
        auto cooked_textures = scene.textures;
        std::vector<scene::StaticScene::TextureMip> cooked_mips;
        auto cooked_bindings = scene.texture_bindings;
        for (auto& binding : cooked_bindings) {
            if (binding.texture >= compression_plans.size()) {
                throw std::runtime_error(
                    "Texture compression binding index is invalid.");
            }
            binding = normalize_texture_binding(
                binding,
                compression_plans[binding.texture]);
        }

        std::ofstream output{
            payload_path,
            std::ios::binary | std::ios::trunc};
        if (!output.is_open()) {
            fail(
                "Failed to open texture payload: ",
                payload_path.generic_string());
        }

        std::uint64_t payload_size = 0;
        try {
            for (std::size_t index = 0;
                 index < texture_paths.size();
                 ++index) {
                const std::filesystem::path path{texture_paths[index]};
                DirectX::TexMetadata metadata{};
                load_texture_metadata(path, metadata);
                validate_texture_metadata(metadata, path);
                if (options.maximum_decoded_texture_bytes != 0 &&
                    estimate_texture_working_memory(
                        metadata,
                        compression_plans[index]) >
                        options.maximum_decoded_texture_bytes) {
                    fail(
                        "Texture exceeds the working memory budget: ",
                        path.generic_string());
                }

                DirectX::ScratchImage decoded;
                load_texture(path, metadata, decoded);
                validate_texture_metadata(metadata, path);
                DirectX::ScratchImage processed;
                process_texture_image(
                    decoded,
                    compression_plans[index],
                    options.fast_bc7,
                    processed);
                metadata = processed.GetMetadata();

                auto& texture = cooked_textures[index];
                texture.width = checked_u32(metadata.width, "Texture width");
                texture.height = checked_u32(metadata.height, "Texture height");
                texture.dxgi_format =
                    static_cast<std::uint32_t>(metadata.format);
                texture.mip_offset = checked_u32(
                    cooked_mips.size(),
                    "Texture mip offset");
                texture.mip_count = checked_u32(
                    metadata.mipLevels,
                    "Texture mip count");
                texture.data_byte_offset = payload_size;

                for (std::size_t mip = 0;
                     mip < metadata.mipLevels;
                     ++mip) {
                    const auto* image = processed.GetImage(mip, 0, 0);
                    if (image == nullptr || image->pixels == nullptr) {
                        fail(
                            "Texture mip is missing: ",
                            path.generic_string());
                    }

                    scene::StaticScene::TextureMip destination;
                    destination.width = checked_u32(
                        image->width,
                        "Texture mip width");
                    destination.height = checked_u32(
                        image->height,
                        "Texture mip height");
                    destination.row_pitch = checked_u32(
                        image->rowPitch,
                        "Texture mip row pitch");
                    destination.slice_pitch = checked_u32(
                        image->slicePitch,
                        "Texture mip slice pitch");
                    destination.data_byte_offset_local =
                        payload_size - texture.data_byte_offset;
                    cooked_mips.push_back(destination);

                    if (image->slicePitch >
                        std::numeric_limits<std::uint64_t>::max() -
                            payload_size) {
                        fail("Texture payload exceeds uint64_t.");
                    }
                    write_bytes(
                        output,
                        reinterpret_cast<const std::byte*>(image->pixels),
                        image->slicePitch,
                        path);
                    payload_size += image->slicePitch;
                }

                texture.data_size =
                    payload_size - texture.data_byte_offset;
            }

            output.flush();
            if (!output) {
                fail(
                    "Failed to flush texture payload: ",
                    payload_path.generic_string());
            }
            output.close();
            if (!output) {
                fail(
                    "Failed to close texture payload: ",
                    payload_path.generic_string());
            }

            scene.textures = std::move(cooked_textures);
            scene.texture_mips = std::move(cooked_mips);
            scene.texture_bindings = std::move(cooked_bindings);
        }
        catch (...) {
            output.close();
            std::error_code error;
            std::filesystem::remove(payload_path, error);
            throw;
        }

        return payload_size;
    }

} // namespace fjr::cooker
