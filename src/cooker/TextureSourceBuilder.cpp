#include "TextureSourceBuilder.hpp"

#include <DirectXTexEXR.h>

#include <Windows.h>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/core/util/Path.hpp"

namespace fjr::cooker {
    namespace {

        enum class TextureFileKind {
            DDS,
            TGA,
            HDR,
            EXR,
            WIC,
        };

        [[nodiscard]] TextureFileKind texture_file_kind(
            const std::filesystem::path& path) {
            const util::Path source{path};
            if (source.has_extension_case_insensitive(".dds")) {
                return TextureFileKind::DDS;
            }
            if (source.has_extension_case_insensitive(".tga")) {
                return TextureFileKind::TGA;
            }
            if (source.has_extension_case_insensitive(".hdr")) {
                return TextureFileKind::HDR;
            }
            if (source.has_extension_case_insensitive(".exr")) {
                return TextureFileKind::EXR;
            }
            return TextureFileKind::WIC;
        }

        void validate_texture(
            const DirectX::TexMetadata& metadata,
            std::string_view source) {
            if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D ||
                metadata.arraySize != 1 ||
                metadata.depth != 1 ||
                metadata.mipLevels == 0) {
                log::fail("Texture is not a single 2D image: ", source);
            }
        }

    } // namespace

    std::string_view texture_source_key(
        const scene::StaticScene& scene,
        uint32_t texture) {
        for (const auto& reference : scene.texture_payload_refs) {
            if (reference.texture != texture) {
                continue;
            }
            if (reference.key >= scene.strings.size()) {
                break;
            }
            return scene.strings.data() + reference.key;
        }
        log::fail("Texture payload key is missing.");
    }

    TextureSource TextureSourceBuilder::build(
        const std::filesystem::path& path) {
        TextureSource result;
        HRESULT status = E_FAIL;
        switch (texture_file_kind(path)) {
        case TextureFileKind::DDS:
            status = DirectX::LoadFromDDSFile(
                path.c_str(),
                DirectX::DDS_FLAGS_NONE,
                &result.metadata,
                result.image);
            break;
        case TextureFileKind::TGA:
            status = DirectX::LoadFromTGAFile(
                path.c_str(),
                &result.metadata,
                result.image);
            break;
        case TextureFileKind::HDR:
            status = DirectX::LoadFromHDRFile(
                path.c_str(),
                &result.metadata,
                result.image);
            break;
        case TextureFileKind::EXR:
            status = DirectX::LoadFromEXRFile(
                path.c_str(),
                &result.metadata,
                result.image);
            break;
        case TextureFileKind::WIC:
            status = DirectX::LoadFromWICFile(
                path.c_str(),
                DirectX::WIC_FLAGS_NONE,
                &result.metadata,
                result.image);
            break;
        }

        if (FAILED(status)) {
            log::fail("Texture decode failed: ", path);
        }
        validate_texture(result.metadata, path.generic_string());
        return result;
    }

    TextureSource TextureSourceBuilder::build(
        const DirectX::ScratchImage& image,
        std::string_view key) {
        TextureSource result;
        const auto* base_image = image.GetImage(0, 0, 0);
        if (base_image == nullptr ||
            FAILED(result.image.InitializeFromImage(*base_image))) {
            log::fail("Generated texture base image is missing: ", key);
        }
        result.metadata = result.image.GetMetadata();
        validate_texture(result.metadata, key);
        return result;
    }

} // namespace fjr::cooker
