#include "FastJungle/cooker/TextureCooker.hpp"

#include <DirectXTex.h>
#if defined(FASTJUNGLE_HAS_OPENEXR)
#include <DirectXTexEXR.h>
#endif

#include <Windows.h>
#include <objbase.h>
#include <algorithm>
#include <cctype>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "FastJungle/cooker/TextureCookPreparation.hpp"
#include "FastJungle/cooker/TextureImageProcessing.hpp"
#include "FastJungle/cooker/TexturePayloadWriter.hpp"
#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/scene/StaticSceneReader.hpp"

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
                    log::Logger::g_logger << log::abrt("CoInitializeEx failed.");
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
                log::Logger::g_logger << log::abrt(
                    "EXR texture support is disabled: " + path.generic_string());
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
                log::Logger::g_logger << log::abrt(
                    "Texture metadata read failed: " + path.generic_string());
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
                log::Logger::g_logger << log::abrt(
                    "EXR texture support is disabled: " + path.generic_string());
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
                log::Logger::g_logger << log::abrt(
                    "Texture decode failed: " + path.generic_string());
            }
        }

        void validate_texture_metadata(
            const DirectX::TexMetadata& metadata,
            const std::filesystem::path& path) {
            if (metadata.dimension != DirectX::TEX_DIMENSION_TEXTURE2D ||
                metadata.arraySize != 1 ||
                metadata.depth != 1 ||
                metadata.mipLevels == 0) {
                log::Logger::g_logger << log::abrt(
                    "Texture is not a single 2D image: " + path.generic_string());
            }
        }

        [[nodiscard]] std::string_view texture_key(
            const scene::StaticScene& scene,
            std::uint32_t texture) {
            const auto found = std::ranges::find_if(
                scene.texture_payload_refs,
                [texture](const auto& reference) {
                    return reference.texture == texture;
                });
            if (found == scene.texture_payload_refs.end() ||
                found->key >= scene.strings.size()) {
                log::Logger::g_logger << log::abrt("Texture payload key is missing.");
            }
            return scene.strings.data() + found->key;
        }

    } // namespace

    std::uint64_t TextureCooker::cook(
        scene::StaticScene& scene,
        const std::filesystem::path& payload_path,
        TextureCookOptions options,
        std::span<const GeneratedTexture> generated_textures) {
        if (!scene.texture_data.empty() || !scene.texture_mips.empty()) {
            log::Logger::g_logger << log::abrt(
                "TextureCooker requires an uncooked StaticScene.");
        }

        const ComScope com_scope;
        const auto preparation = prepare_texture_cook(scene);
        const auto& compression_plans = preparation.compression_plans;
        std::unordered_map<std::string_view, const GeneratedTexture*>
            generated_by_key;
        generated_by_key.reserve(generated_textures.size());
        for (const auto& generated : generated_textures) {
            generated_by_key.emplace(generated.key, &generated);
        }
        std::vector<fjr::scene::StaticScene::Texture> cooked_textures =
            scene.textures;
        std::vector<scene::StaticScene::TextureMip> cooked_mips;
        std::vector<fjr::scene::StaticScene::TextureBinding> cooked_bindings =
            scene.texture_bindings;
        for (auto& binding : cooked_bindings) {
            if (binding.texture >= compression_plans.size()) {
                log::Logger::g_logger << log::abrt(
                    "Texture compression binding index is invalid.");
            }
            binding = normalize_texture_binding(
                binding,
                compression_plans[binding.texture]);
        }
        if (preparation.duplicate_texture_count != 0 ||
            preparation.folded_base_color_alpha_count != 0) {
            log::Logger::g_logger
                << "Texture cook optimizations: "
                << preparation.duplicate_texture_count
                << " duplicate textures removed, "
                << preparation.folded_base_color_alpha_count
                << " base-color alpha samples folded.\n";
        }

        TexturePayloadWriter payload{payload_path};
        std::uint64_t payload_size = 0;
        try {
            for (std::size_t index = 0;
                 index < scene.textures.size();
                 ++index) {
                const auto key = texture_key(
                    scene,
                    static_cast<std::uint32_t>(index));
                const std::filesystem::path path{
                    key};
                DirectX::TexMetadata metadata{};
                const auto generated = generated_by_key.find(
                    key);
                if (generated != generated_by_key.end()) {
                    metadata = generated->second->image.GetMetadata();
                }
                else {
                    load_texture_metadata(path, metadata);
                }
                validate_texture_metadata(metadata, path);
                if (options.maximum_decoded_texture_bytes != 0 &&
                    estimate_texture_working_memory(
                        metadata,
                        compression_plans[index]) >
                        options.maximum_decoded_texture_bytes) {
                    log::Logger::g_logger << log::abrt(
                        "Texture exceeds the working memory budget: " +
                        path.generic_string());
                }

                DirectX::ScratchImage decoded;
                if (generated != generated_by_key.end()) {
                    const auto* image = generated->second->image.GetImage(0, 0, 0);
                    if (image == nullptr ||
                        FAILED(decoded.InitializeFromImage(*image))) {
                        log::Logger::g_logger << log::abrt(
                            "Generated texture base image is missing.");
                    }
                }
                else {
                    load_texture(path, metadata, decoded);
                }
                validate_texture_metadata(metadata, path);
                auto plan = compression_plans[index];
                if (generated != generated_by_key.end() &&
                    generated->second->uncompressed_output_format !=
                        DXGI_FORMAT_UNKNOWN) {
                    plan.dxgi_format = static_cast<std::uint32_t>(
                        generated->second->uncompressed_output_format);
                    plan.filter_as_srgb = false;
                    plan.use_block_compression = false;
                }
                DirectX::ScratchImage processed;
                process_texture_image(
                    decoded,
                    plan,
                    options.fast_bc7,
                    key,
                    processed);
                payload.append(
                    processed,
                    cooked_textures[index],
                    cooked_mips,
                    path);
            }
            payload_size = payload.finish();

            scene.textures = std::move(cooked_textures);
            scene.texture_mips = std::move(cooked_mips);
            scene.texture_bindings = std::move(cooked_bindings);
        }
        catch (...) {
            payload.abandon();
            log::Logger::g_logger << log::abrt(
                "Texture cooking failed.");
        }

        return payload_size;
    }

    std::uint64_t TextureCooker::reuse(
        scene::StaticScene& scene,
        const std::filesystem::path& texture_path) {

        const auto cached = scene::StaticSceneReader::load_texture_metadata(
            texture_path);
        const auto preparation = prepare_texture_cook(scene);
        const auto& compression_plans = preparation.compression_plans;
        auto cooked_textures = scene.textures;
        std::vector<scene::StaticScene::TextureMip> cooked_mips;
        auto cooked_bindings = scene.texture_bindings;

        for (auto& binding : cooked_bindings) {
            binding = normalize_texture_binding(
                binding,
                compression_plans[binding.texture]);
        }
        if (preparation.duplicate_texture_count != 0 ||
            preparation.folded_base_color_alpha_count != 0) {
            log::Logger::g_logger
                << "Texture reuse optimizations: "
                << preparation.duplicate_texture_count
                << " duplicate textures removed, "
                << preparation.folded_base_color_alpha_count
                << " base-color alpha samples folded.\n";
        }

        for (const auto& reference : scene.texture_payload_refs) {
            const auto key = texture_key(scene, reference.texture);
            const auto cached_reference = std::ranges::find_if(
                cached.texture_payload_refs,
                [&cached, key](const auto& candidate) {
                    return candidate.key < cached.strings.size() &&
                        key == cached.strings.data() + candidate.key;
                });
            if (cached_reference == cached.texture_payload_refs.end()) {
                log::Logger::g_logger << log::abrt(
                    std::string{"Cooked texture payload key is missing: "} +
                    std::string{key});
            }

            const auto& source = cached.textures[cached_reference->texture];
            auto& destination = cooked_textures[reference.texture];
            const auto name = destination.name;
            destination = source;
            destination.name = name;
            destination.mip_offset = static_cast<std::uint32_t>(
                cooked_mips.size());

            for (std::uint32_t mip = 0;
                 mip < source.mip_count;
                 ++mip) {
                cooked_mips.push_back(
                    cached.texture_mips[source.mip_offset + mip]);
            }
        }

        scene.textures = std::move(cooked_textures);
        scene.texture_mips = std::move(cooked_mips);
        scene.texture_bindings = std::move(cooked_bindings);
        return cached.texture_payload.size;
    }

} // namespace fjr::cooker
