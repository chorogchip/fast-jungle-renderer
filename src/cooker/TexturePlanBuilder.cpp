#include "TexturePlanBuilder.hpp"
#include "TextureSourceBuilder.hpp"

#include <cmath>
#include <filesystem>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

#include "FastJungle/core/util/EnumUtils.hpp"
#include "FastJungle/core/util/File.hpp"
#include "FastJungle/core/util/FileHash.hpp"
#include "FastJungle/core/util/Logger.hpp"

namespace fjr::cooker {
    namespace {
        [[nodiscard]] bool
        same_compression_plan(const TextureCompressionPlan& left,
                              const TextureCompressionPlan& right) noexcept {
            return left.dxgi_format == right.dxgi_format &&
                   left.source_channel == right.source_channel &&
                   left.isolate_source_channel ==
                       right.isolate_source_channel &&
                   left.linearize_source_channel ==
                       right.linearize_source_channel &&
                   left.filter_as_srgb == right.filter_as_srgb &&
                   left.preserve_alpha_coverage ==
                       right.preserve_alpha_coverage &&
                   left.alpha_reference == right.alpha_reference &&
                   left.use_block_compression == right.use_block_compression;
        }

        [[nodiscard]] bool
        is_generated_texture_key(std::string_view key) noexcept {
            return key.starts_with("generated://");
        }

        void
        deduplicate_textures(scene::StaticScene& scene,
                             const std::vector<TextureCompressionPlan>& plans) {
            const std::size_t texture_count = scene.textures.size();
            std::vector<std::filesystem::path> paths;
            std::vector<uint64_t> source_sizes;
            std::vector<bool> generated(texture_count, false);
            std::vector<std::optional<util::Sha256>> hashes(texture_count);
            paths.reserve(texture_count);
            source_sizes.reserve(texture_count);
            for (std::size_t index = 0; index < texture_count; ++index) {
                const auto key =
                    texture_source_key(scene, static_cast<uint32_t>(index));
                const std::filesystem::path path{key};
                generated[index] = is_generated_texture_key(key);
                if (generated[index]) {
                    paths.emplace_back();
                    source_sizes.push_back(0);
                    continue;
                }
                const auto size = util::File::size(path);
                paths.push_back(path);
                source_sizes.push_back(size);
            }

            const auto hash_at =
                [&hashes, &paths](std::size_t index) -> const util::Sha256& {
                if (!hashes[index]) {
                    hashes[index] = util::FileHash::sha256(paths[index]);
                }
                return *hashes[index];
            };

            std::vector<uint32_t> remap(texture_count);
            std::vector<uint32_t> canonical_sources;
            canonical_sources.reserve(texture_count);
            for (std::size_t index = 0; index < texture_count; ++index) {
                const auto source = static_cast<uint32_t>(index);
                remap[index] = scene::StaticScene::INVALID_INDEX;
                for (const auto canonical : canonical_sources) {
                    if (generated[index] || generated[canonical]) {
                        continue;
                    }
                    if (source_sizes[index] != source_sizes[canonical] ||
                        !same_compression_plan(plans[index],
                                               plans[canonical])) {
                        continue;
                    }
                    if (hash_at(index) == hash_at(canonical)) {
                        remap[index] = remap[canonical];
                        break;
                    }
                }
                if (remap[index] == scene::StaticScene::INVALID_INDEX) {
                    remap[index] =
                        static_cast<uint32_t>(canonical_sources.size());
                    canonical_sources.push_back(source);
                }
            }

            if (canonical_sources.size() == texture_count) {
                return;
            }

            std::vector<scene::StaticScene::Texture> textures;
            std::vector<scene::StaticScene::TexturePayloadRef> payload_refs;
            textures.reserve(canonical_sources.size());
            payload_refs.reserve(canonical_sources.size());
            for (const auto source : canonical_sources) {
                textures.push_back(scene.textures[source]);
                const auto reference =
                    std::ranges::find_if(scene.texture_payload_refs,
                                         [source](const auto& candidate) {
                                             return candidate.texture == source;
                                         });
                if (reference == scene.texture_payload_refs.end()) {
                    log::Logger::g_logger
                        << log::abrt("Texture payload reference is missing "
                                     "during deduplication.");
                }
                scene::StaticScene::TexturePayloadRef destination_reference =
                    *reference;
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
        }

        struct OpacityVariant final {
            uint32_t source_texture = scene::StaticScene::INVALID_INDEX;
            scene::StaticScene::EnumTextureChannel source_channel =
                scene::StaticScene::EnumTextureChannel::R;
            scene::StaticScene::EnumTextureBindingFlag source_flags =
                scene::StaticScene::EnumTextureBindingFlag::LINEAR;
            uint32_t destination_texture = scene::StaticScene::INVALID_INDEX;

            [[nodiscard]] bool
            same_source(const OpacityVariant& other) const noexcept {
                return source_texture == other.source_texture &&
                       source_channel == other.source_channel &&
                       source_flags == other.source_flags;
            }
        };

        struct PendingAlphaMaterial final {
            uint32_t material = scene::StaticScene::INVALID_INDEX;
            scene::StaticScene::TextureBinding base;
            scene::StaticScene::TextureBinding opacity;
            std::size_t opacity_variant = 0;
        };

        [[nodiscard]] std::vector<bool>
        alpha_tested_materials(const scene::StaticScene& scene) {
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

        [[nodiscard]] scene::StaticScene::TextureBinding
        binding_at(const scene::StaticScene& scene, uint32_t binding) {
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

        [[nodiscard]] uint32_t clone_texture(scene::StaticScene& scene,
                                             uint32_t source) {
            if (source >= scene.textures.size()) {
                log::Logger::g_logger
                    << log::abrt("Opacity source texture index is invalid.");
            }
            const auto reference = std::ranges::find_if(
                scene.texture_payload_refs, [source](const auto& candidate) {
                    return candidate.texture == source;
                });
            if (reference == scene.texture_payload_refs.end()) {
                log::Logger::g_logger << log::abrt(
                    "Opacity source texture payload reference is missing.");
            }
            const auto destination =
                static_cast<uint32_t>(scene.textures.size());
            scene.textures.push_back(scene.textures[source]);
            auto cloned_reference = *reference;
            cloned_reference.texture = destination;
            scene.texture_payload_refs.push_back(cloned_reference);
            return destination;
        }

        void separate_alpha_tested_textures(scene::StaticScene& scene) {
            constexpr float ALPHA_REFERENCE = 0.5f;
            const auto alpha_tested = alpha_tested_materials(scene);
            const auto source_texture_count = scene.textures.size();
            std::vector<bool> used_outside_alpha_opacity(source_texture_count,
                                                         false);

            const auto mark_other_usage =
                [&scene, &used_outside_alpha_opacity](uint32_t binding) {
                    if (binding == scene::StaticScene::INVALID_INDEX) {
                        return;
                    }
                    const auto texture = binding_at(scene, binding).texture;
                    used_outside_alpha_opacity[texture] = true;
                };
            for (std::size_t index = 0; index < scene.materials.size();
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
                    log::Logger::g_logger
                        << log::abrt("Environment texture index is invalid.");
                }
                used_outside_alpha_opacity[scene.environment_light.texture] =
                    true;
            }
            std::vector<OpacityVariant> variants;
            std::vector<PendingAlphaMaterial> pending;
            for (std::size_t index = 0; index < scene.materials.size();
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

                auto base =
                    binding_at(scene, material.texture_binding_base_color);
                scene::StaticScene::TextureBinding opacity;
                if (material.texture_binding_opacity ==
                    scene::StaticScene::INVALID_INDEX) {
                    if (base.channel !=
                        scene::StaticScene::EnumTextureChannel::RGBA) {
                        log::Logger::g_logger << log::abrt(
                            "Alpha-tested material has no opacity source.");
                    }
                    opacity = base;
                    opacity.channel = scene::StaticScene::EnumTextureChannel::A;
                    opacity.flags =
                        scene::StaticScene::EnumTextureBindingFlag::LINEAR;
                } else {
                    opacity =
                        binding_at(scene, material.texture_binding_opacity);
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
                    variants, [&key](const auto& candidate) {
                        return candidate.same_source(key);
                    });
                std::size_t variant = 0;
                if (found == variants.end()) {
                    variant = variants.size();
                    variants.push_back(key);
                } else {
                    variant = static_cast<std::size_t>(
                        std::distance(variants.begin(), found));
                }
                pending.push_back({
                    .material = static_cast<uint32_t>(index),
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
                } else {
                    variant.destination_texture =
                        clone_texture(scene, variant.source_texture);
                }
            }

            for (const auto& item : pending) {
                auto& material = scene.materials[item.material];
                material.opacity_threshold = ALPHA_REFERENCE;
                material.texture_binding_base_color =
                    static_cast<uint32_t>(scene.texture_bindings.size());
                scene.texture_bindings.push_back(item.base);

                auto opacity = item.opacity;
                opacity.texture =
                    variants[item.opacity_variant].destination_texture;
                material.texture_binding_opacity =
                    static_cast<uint32_t>(scene.texture_bindings.size());
                scene.texture_bindings.push_back(opacity);
            }
        }

    } // namespace

    std::vector<TextureCompressionPlan>
    TexturePlanBuilder::build(scene::StaticScene& scene) {
        separate_alpha_tested_textures(scene);
        const auto pre_deduplication_plans = resolve_texture_compression(scene);
        deduplicate_textures(scene, pre_deduplication_plans);
        return resolve_texture_compression(scene);
    }

} // namespace fjr::cooker
