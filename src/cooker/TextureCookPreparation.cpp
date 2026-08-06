#include "FastJungle/cooker/TextureCookPreparation.hpp"

#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ranges>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "FastJungle/cooker/CookerCommon.hpp"

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
                fail("Texture payload key is missing.");
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
                    fail("Texture content hash operation failed: ", operation);
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
                fail("Texture content hash object size is invalid.");
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
                fail("Failed to open texture for content hashing: ",
                    path.generic_string());
            }
            std::array<UCHAR, 1024 * 1024> buffer{};
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
                fail("Failed to read texture for content hashing: ",
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
                left.filter_as_srgb == right.filter_as_srgb;
        }

        [[nodiscard]] std::uint32_t deduplicate_textures(
            scene::StaticScene& scene,
            const std::vector<TextureCompressionPlan>& plans) {
            const std::size_t texture_count = scene.textures.size();
            std::vector<std::filesystem::path> paths;
            std::vector<std::uint64_t> source_sizes;
            std::vector<std::optional<ContentHash>> hashes(texture_count);
            paths.reserve(texture_count);
            source_sizes.reserve(texture_count);
            for (std::size_t index = 0; index < texture_count; ++index) {
                const std::filesystem::path path{
                    texture_key(scene, checked_u32(index, "Texture index"))};
                std::error_code error;
                const auto size = std::filesystem::file_size(path, error);
                if (error) {
                    fail("Failed to get texture source size: ",
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
                const auto source = checked_u32(index, "Texture index");
                remap[index] = scene::StaticScene::INVALID_INDEX;
                for (const auto canonical : canonical_sources) {
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
                    remap[index] = checked_u32(
                        canonical_sources.size(),
                        "Deduplicated texture index");
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
                    fail("Texture payload reference is missing during deduplication.");
                }
                auto destination_reference = *reference;
                destination_reference.texture = remap[source];
                payload_refs.push_back(destination_reference);
            }

            for (auto& binding : scene.texture_bindings) {
                binding.texture = remap[binding.texture];
            }
            if (scene.environment_light.texture !=
                scene::StaticScene::INVALID_INDEX) {
                scene.environment_light.texture =
                    remap[scene.environment_light.texture];
            }
            scene.textures = std::move(textures);
            scene.texture_payload_refs = std::move(payload_refs);
            return checked_u32(
                texture_count - canonical_sources.size(),
                "Deduplicated texture count");
        }

        [[nodiscard]] std::uint32_t fold_base_color_alpha(
            scene::StaticScene& scene) {
            std::uint32_t result = 0;
            for (auto& material : scene.materials) {
                const auto base_id = material.texture_binding_base_color;
                const auto opacity_id = material.texture_binding_opacity;
                if (base_id == scene::StaticScene::INVALID_INDEX ||
                    opacity_id == scene::StaticScene::INVALID_INDEX) {
                    continue;
                }
                auto& base = scene.texture_bindings[base_id];
                const auto& opacity = scene.texture_bindings[opacity_id];
                if (base.texture != opacity.texture ||
                    base.sampler != opacity.sampler ||
                    base.channel !=
                        scene::StaticScene::EnumTextureChannel::RGB ||
                    opacity.channel !=
                        scene::StaticScene::EnumTextureChannel::A) {
                    continue;
                }
                base.channel = scene::StaticScene::EnumTextureChannel::RGBA;
                material.texture_binding_opacity =
                    scene::StaticScene::INVALID_INDEX;
                ++result;
            }
            return result;
        }

    } // namespace

    TextureCookPreparation prepare_texture_cook(
        scene::StaticScene& scene) {
        TextureCookPreparation result;
        result.folded_base_color_alpha_count = fold_base_color_alpha(scene);
        const auto pre_deduplication_plans =
            resolve_texture_compression(scene);
        result.duplicate_texture_count = deduplicate_textures(
            scene,
            pre_deduplication_plans);
        result.compression_plans = resolve_texture_compression(scene);
        return result;
    }

} // namespace fjr::cooker
