
#include <dxgiformat.h>
#include <algorithm>
#include <array>

#include "FastJungle/core/util/EnumUtils.hpp"
#include "FastJungle/core/util/Logger.hpp"
#include "TextureCompression.hpp"

namespace fjr::cooker {
    namespace {
        enum class TextureUsage : uint32_t {
            None = 0,
            BaseColor = 1u << 0u,
            Emissive = 1u << 1u,
            Normal = 1u << 2u,
            Scalar = 1u << 3u,
            Environment = 1u << 4u,
            Opacity = 1u << 5u,
        };

        struct UsageRecord final {
            uint32_t usage = 0;
            std::array<bool, 5> scalar_channels{};
            bool base_color_uses_alpha = false;
            bool preserve_alpha_coverage = false;
            bool full_vector_normal = false;
            bool srgb = false;
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

        [[nodiscard]] std::size_t channel_index(
            scene::StaticScene::EnumTextureChannel channel) {
            const auto index = static_cast<std::size_t>(channel);
            if (index >= 5) {
                log::Logger::g_logger << log::abrt(
                    "Texture binding channel is invalid.");
            }
            return index;
        }

        void add_usage(
            const scene::StaticScene& scene,
            std::vector<UsageRecord>& usages,
            uint32_t binding_index,
            TextureUsage usage) {
            if (binding_index == scene::StaticScene::INVALID_INDEX) {
                return;
            }
            if (binding_index >= scene.texture_bindings.size()) {
                log::Logger::g_logger << log::abrt(
                    "Texture compression binding index is invalid.");
            }

            const auto& binding = scene.texture_bindings[binding_index];
            if (binding.texture >= usages.size()) {
                log::Logger::g_logger << log::abrt(
                    "Texture compression texture index is invalid.");
            }

            auto usage_i = static_cast<uint32_t>(usage);
            const bool opacity = usage == TextureUsage::Opacity;
            if (opacity) {
                usage_i |= static_cast<uint32_t>(TextureUsage::Scalar);
            }
            auto& record = usages[binding.texture];
            record.usage |= usage_i;
            record.srgb = record.srgb || binding.flags == scene::StaticScene::EnumTextureBindingFlag::SRGB;
            record.preserve_alpha_coverage =
                record.preserve_alpha_coverage || opacity;
            if (usage_i & static_cast<uint32_t>(TextureUsage::BaseColor)) {
                record.base_color_uses_alpha =
                    record.base_color_uses_alpha ||
                    binding.channel != scene::StaticScene::EnumTextureChannel::RGB;
            }
            if (usage_i & static_cast<uint32_t>(TextureUsage::Scalar)) {
                record.scalar_channels[channel_index(binding.channel)] = true;
            }
        }

        [[nodiscard]] scene::StaticScene::EnumTextureChannel scalar_channel(
            const UsageRecord& usage) noexcept {
            for (size_t index = 0; index < usage.scalar_channels.size(); ++index) {
                if (usage.scalar_channels[index]) {
                    return static_cast<scene::StaticScene::EnumTextureChannel>(index);
                }
            }
            return scene::StaticScene::EnumTextureChannel::R;
        }

        void mark_impostor_normals(
            const scene::StaticScene& scene,
            std::vector<UsageRecord>& usages) {
            for (const auto& impostor : scene.impostors) {
                for (uint32_t direction = 0;
                    direction < impostor.direction_count;
                    ++direction) {
                    const auto& mesh = scene.meshes[
                        impostor.card_mesh_offset + direction];
                    const auto& lod = scene.mesh_lods[mesh.lod_offset];
                    const auto& submesh = scene.submeshes[lod.submesh_offset];
                    const auto& material = scene.materials[submesh.material];
                    const auto& binding = scene.texture_bindings[
                        material.texture_binding_normal];
                    usages[binding.texture].full_vector_normal = true;
                }
            }
        }

    } // namespace

    std::vector<TextureCompressionPlan>
    resolve_texture_compression(const scene::StaticScene& scene) {
        std::vector<UsageRecord> usages(scene.textures.size());
        const auto alpha_tested = alpha_tested_materials(scene);

        for (std::size_t material_index = 0;
            material_index < scene.materials.size();
            ++material_index) {
            const auto& material = scene.materials[material_index];
            add_usage(
                scene,
                usages,
                material.texture_binding_base_color,
                TextureUsage::BaseColor);
            add_usage(
                scene,
                usages,
                material.texture_binding_normal,
                TextureUsage::Normal);
            add_usage(
                scene,
                usages,
                material.texture_binding_roughness,
                TextureUsage::Scalar);
			add_usage(
				scene,
				usages,
				material.texture_binding_metallic,
				TextureUsage::Scalar);
            add_usage(
                scene,
                usages,
                material.texture_binding_opacity,
                alpha_tested[material_index]
                    ? TextureUsage::Opacity
                    : TextureUsage::Scalar);
            add_usage(
                scene,
                usages,
                material.texture_binding_emissive,
                TextureUsage::Emissive);
        }

        if (scene.environment_light.texture !=
            scene::StaticScene::INVALID_INDEX) {
            if (scene.environment_light.texture >= usages.size()) {
                log::Logger::g_logger << log::abrt(
                    "Environment texture index is invalid.");
            }
            usages[scene.environment_light.texture].usage |=
                static_cast<uint32_t>(TextureUsage::Environment);
        }

        mark_impostor_normals(scene, usages);

        std::vector<TextureCompressionPlan> result;
        result.reserve(usages.size());
        for (const auto& usage : usages) {
            TextureCompressionPlan plan;
            const auto contains = [&usage](TextureUsage expected) {
                return enm::has(
                    usage.usage,
                    static_cast<uint32_t>(expected));
            };
            const bool environment = contains(TextureUsage::Environment);
            const bool base_color = contains(TextureUsage::BaseColor);
            const bool emissive = contains(TextureUsage::Emissive);
            const bool normal = contains(TextureUsage::Normal);
            const bool scalar = contains(TextureUsage::Scalar);

            if (environment && !base_color && !emissive && !normal && !scalar) {
                plan.dxgi_format = DXGI_FORMAT_BC6H_UF16;
            }
            else if (normal && !base_color && !emissive && !scalar && !environment) {
                plan.dxgi_format = usage.full_vector_normal
                    ? DXGI_FORMAT_BC7_UNORM
                    : DXGI_FORMAT_BC5_UNORM;
            }
            else if (scalar && !base_color && !emissive && !normal && !environment &&
                std::ranges::count(usage.scalar_channels, true) == 1) {
                plan.dxgi_format = DXGI_FORMAT_BC4_UNORM;
                plan.source_channel = scalar_channel(usage);
                plan.isolate_source_channel = true;
                plan.linearize_source_channel = usage.srgb;
                plan.preserve_alpha_coverage =
                    usage.preserve_alpha_coverage;
            }
            else if (base_color && !usage.base_color_uses_alpha &&
                !emissive && !normal && !scalar &&
                !environment) {
                // An RGB-only base-color map needs neither a continuous
                // alpha channel nor the extra precision of BC7. BC1 halves
                // its resident and payload size, while the SRGB view still
                // comes from the texture binding at runtime.
                plan.dxgi_format = DXGI_FORMAT_BC1_UNORM;
                plan.filter_as_srgb = usage.srgb;
            }
            else {
                plan.dxgi_format = DXGI_FORMAT_BC7_UNORM;
                plan.filter_as_srgb = usage.srgb;
            }
            if (usage.preserve_alpha_coverage &&
                (!plan.isolate_source_channel ||
                    plan.dxgi_format != DXGI_FORMAT_BC4_UNORM)) {
                log::Logger::g_logger << log::abrt(
                    "Alpha-tested opacity texture is not a dedicated scalar texture.");
            }
            result.push_back(plan);
        }
        return result;
    }

    scene::StaticScene::TextureBinding
    normalize_texture_binding(
        scene::StaticScene::TextureBinding binding,
        const TextureCompressionPlan& plan) noexcept {
        if (plan.isolate_source_channel) {
            binding.channel = scene::StaticScene::EnumTextureChannel::R;
            binding.flags = scene::StaticScene::EnumTextureBindingFlag::LINEAR;
        }
        return binding;
    }

} // namespace fjr::cooker
