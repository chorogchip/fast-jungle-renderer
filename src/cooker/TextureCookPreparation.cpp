#include "FastJungle/cooker/TextureCookPreparation.hpp"

#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ranges>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/core/util/EnumUtils.hpp"

namespace fjr::cooker {
    namespace {
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

        struct ContentHash final {
            std::array<UCHAR, 32> bytes{};

            [[nodiscard]] bool operator==(
                const ContentHash&) const noexcept = default;
        };

        [[nodiscard]] ContentHash content_hash(
            const std::filesystem::path& path) {
            BCRYPT_ALG_HANDLE algorithm = nullptr;
            BCRYPT_HASH_HANDLE hash = nullptr;
            std::vector<UCHAR> hash_object;
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
                    log::Logger::g_logger << log::abrt(
                        std::string{"Texture content hash operation failed: "} +
                        std::string{operation});
                }
            };

            require_success(
                BCryptOpenAlgorithmProvider(
                    &algorithm,
                    BCRYPT_SHA256_ALGORITHM,
                    nullptr,
                    0),
                "open");

            DWORD hash_object_bytes = 0;
            DWORD property_bytes = 0;
            require_success(
                BCryptGetProperty(
                    algorithm,
                    BCRYPT_OBJECT_LENGTH,
                    reinterpret_cast<PUCHAR>(&hash_object_bytes),
                    sizeof(hash_object_bytes),
                    &property_bytes,
                    0),
                "object-size");
            if (property_bytes != sizeof(hash_object_bytes)) {
                close();
                log::Logger::g_logger << log::abrt(
                    "Texture content hash object size is invalid.");
            }
            hash_object.resize(hash_object_bytes);
            require_success(
                BCryptCreateHash(
                    algorithm,
                    &hash,
                    hash_object.data(),
                    hash_object_bytes,
                    nullptr,
                    0,
                    0),
                "create");

            std::ifstream input{path, std::ios::binary};
            if (!input.is_open()) {
                close();
                log::Logger::g_logger << log::abrt(
                    "Failed to open texture for content hashing: " +
                    path.generic_string());
            }
            std::vector<UCHAR> buffer(1024 * 1024);
            while (input) {
                input.read(
                    reinterpret_cast<char*>(buffer.data()),
                    static_cast<std::streamsize>(buffer.size()));
                const auto byte_count = input.gcount();
                if (byte_count > 0) {
                    require_success(
                        BCryptHashData(
                            hash,
                            buffer.data(),
                            static_cast<ULONG>(byte_count),
                            0),
                        "update");
                }
            }
            if (!input.eof()) {
                close();
                log::Logger::g_logger << log::abrt(
                    "Failed to read texture for content hashing: " +
                    path.generic_string());
            }

            ContentHash result;
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

        [[nodiscard]] bool same_compression_plan(
            const TextureCompressionPlan& left,
            const TextureCompressionPlan& right) noexcept {
            return left.dxgi_format == right.dxgi_format &&
                left.source_channel == right.source_channel &&
                left.isolate_source_channel == right.isolate_source_channel &&
                left.linearize_source_channel ==
                    right.linearize_source_channel &&
                left.filter_as_srgb == right.filter_as_srgb &&
                left.preserve_alpha_coverage ==
                    right.preserve_alpha_coverage &&
                left.alpha_reference == right.alpha_reference &&
                left.use_block_compression == right.use_block_compression;
        }

        [[nodiscard]] bool is_generated_texture_key(
            std::string_view key) noexcept {
            return key.starts_with("generated://");
        }

        [[nodiscard]] std::uint32_t deduplicate_textures(
            scene::StaticScene& scene,
            const std::vector<TextureCompressionPlan>& plans) {
            const std::size_t texture_count = scene.textures.size();
            std::vector<std::filesystem::path> paths;
            std::vector<std::uint64_t> source_sizes;
            std::vector<bool> generated(texture_count, false);
            std::vector<std::optional<ContentHash>> hashes(texture_count);
            paths.reserve(texture_count);
            source_sizes.reserve(texture_count);
            for (std::size_t index = 0; index < texture_count; ++index) {
                const auto key = texture_key(
                    scene,
                    static_cast<std::uint32_t>(index));
                const std::filesystem::path path{
                    key};
                generated[index] = is_generated_texture_key(key);
                if (generated[index]) {
                    paths.emplace_back();
                    source_sizes.push_back(0);
                    continue;
                }
                std::error_code error;
                const auto size = std::filesystem::file_size(path, error);
                if (error) {
                    log::Logger::g_logger << log::abrt(
                        "Failed to get texture source size: " +
                        path.generic_string());
                }
                paths.push_back(path);
                source_sizes.push_back(size);
            }

            const auto hash_at = [&hashes, &paths](std::size_t index)
                -> const ContentHash& {
                if (!hashes[index]) {
                    hashes[index] = content_hash(paths[index]);
                }
                return *hashes[index];
            };

            std::vector<std::uint32_t> remap(texture_count);
            std::vector<std::uint32_t> canonical_sources;
            canonical_sources.reserve(texture_count);
            for (std::size_t index = 0; index < texture_count; ++index) {
                const auto source = static_cast<std::uint32_t>(index);
                remap[index] = scene::StaticScene::INVALID_INDEX;
                for (const auto canonical : canonical_sources) {
                    if (generated[index] || generated[canonical]) {
                        continue;
                    }
                    if (source_sizes[index] != source_sizes[canonical] ||
                        !same_compression_plan(plans[index], plans[canonical])) {
                        continue;
                    }
                    if (hash_at(index) == hash_at(canonical)) {
                        remap[index] = remap[canonical];
                        break;
                    }
                }
                if (remap[index] == scene::StaticScene::INVALID_INDEX) {
                    remap[index] = static_cast<std::uint32_t>(
                        canonical_sources.size());
                    canonical_sources.push_back(source);
                }
            }

            if (canonical_sources.size() == texture_count) {
                return 0;
            }

            std::vector<scene::StaticScene::Texture> textures;
            std::vector<scene::StaticScene::TexturePayloadRef> payload_refs;
            textures.reserve(canonical_sources.size());
            payload_refs.reserve(canonical_sources.size());
            for (const auto source : canonical_sources) {
                textures.push_back(scene.textures[source]);
                const auto reference = std::ranges::find_if(
                    scene.texture_payload_refs,
                    [source](const auto& candidate) {
                        return candidate.texture == source;
                    });
                if (reference == scene.texture_payload_refs.end()) {
                    log::Logger::g_logger << log::abrt(
                        "Texture payload reference is missing during deduplication.");
                }
                scene::StaticScene::TexturePayloadRef destination_reference = *reference;
                destination_reference.texture = remap[source];
                payload_refs.push_back(destination_reference);
            }

            for (auto& binding : scene.texture_bindings) {
                binding.texture = remap[binding.texture];
            }
            for (auto& impostor : scene.impostors) {
                impostor.depth_texture_offset =
                    remap[impostor.depth_texture_offset];
            }
            if (scene.environment_light.texture !=
                scene::StaticScene::INVALID_INDEX) {
                scene.environment_light.texture =
                    remap[scene.environment_light.texture];
            }
            scene.textures = std::move(textures);
            scene.texture_payload_refs = std::move(payload_refs);
            return static_cast<std::uint32_t>(
                texture_count - canonical_sources.size());
        }

        struct OpacityVariant final {
            std::uint32_t source_texture = scene::StaticScene::INVALID_INDEX;
            scene::StaticScene::EnumTextureChannel source_channel =
                scene::StaticScene::EnumTextureChannel::R;
            scene::StaticScene::EnumTextureBindingFlag source_flags =
                scene::StaticScene::EnumTextureBindingFlag::LINEAR;
            std::uint32_t destination_texture =
                scene::StaticScene::INVALID_INDEX;

            [[nodiscard]] bool same_source(
                const OpacityVariant& other) const noexcept {
                return source_texture == other.source_texture &&
                    source_channel == other.source_channel &&
                    source_flags == other.source_flags;
            }
        };

        struct PendingAlphaMaterial final {
            std::uint32_t material = scene::StaticScene::INVALID_INDEX;
            scene::StaticScene::TextureBinding base;
            scene::StaticScene::TextureBinding opacity;
            std::size_t opacity_variant = 0;
        };

        [[nodiscard]] std::vector<bool> alpha_tested_materials(
            const scene::StaticScene& scene) {
            std::vector<bool> result(scene.materials.size(), false);
            for (const auto& submesh : scene.submeshes) {
                if (submesh.material != scene::StaticScene::INVALID_INDEX &&
                    submesh.material < result.size() &&
                    enm::has(
                        submesh.flags,
                        scene::StaticScene::EnumSubmeshFlag::ALPHA_TESTED)) {
                    result[submesh.material] = true;
                }
            }
            return result;
        }

        [[nodiscard]] scene::StaticScene::TextureBinding binding_at(
            const scene::StaticScene& scene,
            std::uint32_t binding) {
            if (binding == scene::StaticScene::INVALID_INDEX ||
                binding >= scene.texture_bindings.size()) {
                log::Logger::g_logger << log::abrt(
                    "Alpha-tested material texture binding is invalid.");
            }
            const auto result = scene.texture_bindings[binding];
            if (result.texture >= scene.textures.size()) {
                log::Logger::g_logger << log::abrt(
                    "Alpha-tested material texture index is invalid.");
            }
            return result;
        }

        [[nodiscard]] std::uint32_t clone_texture(
            scene::StaticScene& scene,
            std::uint32_t source) {
            if (source >= scene.textures.size()) {
                log::Logger::g_logger << log::abrt(
                    "Opacity source texture index is invalid.");
            }
            const auto reference = std::ranges::find_if(
                scene.texture_payload_refs,
                [source](const auto& candidate) {
                    return candidate.texture == source;
                });
            if (reference == scene.texture_payload_refs.end()) {
                log::Logger::g_logger << log::abrt(
                    "Opacity source texture payload reference is missing.");
            }
            const auto destination = static_cast<std::uint32_t>(
                scene.textures.size());
            scene.textures.push_back(scene.textures[source]);
            auto cloned_reference = *reference;
            cloned_reference.texture = destination;
            scene.texture_payload_refs.push_back(cloned_reference);
            return destination;
        }

        [[nodiscard]] std::uint32_t separate_alpha_tested_textures(
            scene::StaticScene& scene) {
            constexpr float ALPHA_REFERENCE = 0.5f;
            const auto alpha_tested = alpha_tested_materials(scene);
            const auto source_texture_count = scene.textures.size();
            std::vector<bool> used_outside_alpha_opacity(
                source_texture_count,
                false);

            const auto mark_other_usage = [&scene, &used_outside_alpha_opacity](
                std::uint32_t binding) {
                if (binding == scene::StaticScene::INVALID_INDEX) {
                    return;
                }
                const auto texture = binding_at(scene, binding).texture;
                used_outside_alpha_opacity[texture] = true;
            };
            for (std::size_t index = 0;
                index < scene.materials.size();
                ++index) {
                const auto& material = scene.materials[index];
                mark_other_usage(material.texture_binding_base_color);
                mark_other_usage(material.texture_binding_normal);
                mark_other_usage(material.texture_binding_roughness);
                mark_other_usage(material.texture_binding_metallic);
                mark_other_usage(material.texture_binding_emissive);
                if (!alpha_tested[index]) {
                    mark_other_usage(material.texture_binding_opacity);
                }
            }
            if (scene.environment_light.texture !=
                scene::StaticScene::INVALID_INDEX) {
                if (scene.environment_light.texture >= source_texture_count) {
                    log::Logger::g_logger << log::abrt(
                        "Environment texture index is invalid.");
                }
                used_outside_alpha_opacity[
                    scene.environment_light.texture] = true;
            }
            for (const auto& impostor : scene.impostors) {
                if (impostor.depth_texture_offset >= source_texture_count) {
                    log::Logger::g_logger << log::abrt(
                        "Impostor depth texture index is invalid.");
                }
                for (std::uint32_t direction = 0;
                    direction < impostor.direction_count;
                    ++direction) {
                    const auto texture = impostor.depth_texture_offset + direction;
                    if (texture >= source_texture_count) {
                        log::Logger::g_logger << log::abrt(
                            "Impostor depth texture range is invalid.");
                    }
                    used_outside_alpha_opacity[texture] = true;
                }
            }

            std::vector<OpacityVariant> variants;
            std::vector<PendingAlphaMaterial> pending;
            for (std::size_t index = 0;
                index < scene.materials.size();
                ++index) {
                if (!alpha_tested[index]) {
                    continue;
                }
                const auto& material = scene.materials[index];
                if (std::abs(material.opacity_threshold - ALPHA_REFERENCE) >
                    1.0e-6f) {
                    log::Logger::g_logger << log::abrt(
                        "Alpha-tested material cutoff is not 0.5.");
                }

                auto base = binding_at(
                    scene,
                    material.texture_binding_base_color);
                scene::StaticScene::TextureBinding opacity;
                if (material.texture_binding_opacity ==
                    scene::StaticScene::INVALID_INDEX) {
                    if (base.channel !=
                        scene::StaticScene::EnumTextureChannel::RGBA) {
                        log::Logger::g_logger << log::abrt(
                            "Alpha-tested material has no opacity source.");
                    }
                    opacity = base;
                    opacity.channel =
                        scene::StaticScene::EnumTextureChannel::A;
                    opacity.flags =
                        scene::StaticScene::EnumTextureBindingFlag::LINEAR;
                }
                else {
                    opacity = binding_at(
                        scene,
                        material.texture_binding_opacity);
                }

                if (base.channel !=
                        scene::StaticScene::EnumTextureChannel::RGB &&
                    base.channel !=
                        scene::StaticScene::EnumTextureChannel::RGBA) {
                    log::Logger::g_logger << log::abrt(
                        "Alpha-tested base-color binding is not RGB/RGBA.");
                }
                if (opacity.channel ==
                        scene::StaticScene::EnumTextureChannel::RGBA ||
                    opacity.channel ==
                        scene::StaticScene::EnumTextureChannel::RGB) {
                    log::Logger::g_logger << log::abrt(
                        "Alpha-tested opacity binding is not scalar.");
                }
                base.channel = scene::StaticScene::EnumTextureChannel::RGB;
                if (opacity.channel ==
                    scene::StaticScene::EnumTextureChannel::A) {
                    opacity.flags =
                        scene::StaticScene::EnumTextureBindingFlag::LINEAR;
                }

                const OpacityVariant key{
                    .source_texture = opacity.texture,
                    .source_channel = opacity.channel,
                    .source_flags = opacity.flags,
                };
                const auto found = std::ranges::find_if(
                    variants,
                    [&key](const auto& candidate) {
                        return candidate.same_source(key);
                    });
                std::size_t variant = 0;
                if (found == variants.end()) {
                    variant = variants.size();
                    variants.push_back(key);
                }
                else {
                    variant = static_cast<std::size_t>(
                        std::distance(variants.begin(), found));
                }
                pending.push_back({
                    .material = static_cast<std::uint32_t>(index),
                    .base = base,
                    .opacity = opacity,
                    .opacity_variant = variant,
                });
            }

            std::vector<bool> reused_source(source_texture_count, false);
            for (auto& variant : variants) {
                if (!used_outside_alpha_opacity[variant.source_texture] &&
                    !reused_source[variant.source_texture]) {
                    variant.destination_texture = variant.source_texture;
                    reused_source[variant.source_texture] = true;
                }
                else {
                    variant.destination_texture = clone_texture(
                        scene,
                        variant.source_texture);
                }
            }

            for (const auto& item : pending) {
                auto& material = scene.materials[item.material];
                material.opacity_threshold = ALPHA_REFERENCE;
                material.texture_binding_base_color =
                    static_cast<std::uint32_t>(scene.texture_bindings.size());
                scene.texture_bindings.push_back(item.base);

                auto opacity = item.opacity;
                opacity.texture = variants[
                    item.opacity_variant].destination_texture;
                material.texture_binding_opacity =
                    static_cast<std::uint32_t>(scene.texture_bindings.size());
                scene.texture_bindings.push_back(opacity);
            }
            return static_cast<std::uint32_t>(pending.size());
        }

    } // namespace

    TextureCookPreparation prepare_texture_cook(
        scene::StaticScene& scene) {
        TextureCookPreparation result;
        result.separated_alpha_tested_material_count =
            separate_alpha_tested_textures(scene);
        const auto pre_deduplication_plans =
            resolve_texture_compression(scene);
        result.duplicate_texture_count = deduplicate_textures(
            scene,
            pre_deduplication_plans);
        result.compression_plans = resolve_texture_compression(scene);
        return result;
    }

} // namespace fjr::cooker
