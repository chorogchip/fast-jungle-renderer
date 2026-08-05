
#include <dxgiformat.h>
#include <algorithm>
#include <array>
#include <stdexcept>

#include "FastJungle/cooker/TextureCompression.hpp"

namespace fjr::cooker {
    namespace {
        enum class TextureUsage : std::uint32_t {
            None = 0,
            Color = 1u << 0u,
            Normal = 1u << 1u,
            Scalar = 1u << 2u,
            Environment = 1u << 3u
        };

        struct UsageRecord final {
            std::uint32_t usage = 0;
            std::array<bool, 5> scalar_channels{};
            bool srgb = false;
        };

        [[nodiscard]] std::size_t channel_index(
            scene::StaticScene::EnumTextureChannel channel) {
            const auto index = static_cast<std::size_t>(channel);
            if (index >= 5) {
                throw std::runtime_error(
                    "Texture binding channel is invalid.");
            }
            return index;
        }

        void add_usage(
            const scene::StaticScene& scene,
            std::vector<UsageRecord>& usages,
            std::uint32_t binding_index,
            TextureUsage usage) {
            if (binding_index == scene::StaticScene::INVALID_INDEX) {
                return;
            }
            if (binding_index >= scene.texture_bindings.size()) {
                throw std::runtime_error(
                    "Texture compression binding index is invalid.");
            }

            const auto& binding = scene.texture_bindings[binding_index];
            if (binding.texture >= usages.size()) {
                throw std::runtime_error(
                    "Texture compression texture index is invalid.");
            }

            auto usage_i = static_cast<uint32_t>(usage);
            auto& record = usages[binding.texture];
            record.usage |= usage_i;
            record.srgb = record.srgb || binding.flags == scene::StaticScene::EnumTextureBindingFlag::SRGB;
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

    } // namespace

    std::vector<TextureCompressionPlan>
    resolve_texture_compression(const scene::StaticScene& scene) {
        std::vector<UsageRecord> usages(scene.textures.size());

        for (const auto& material : scene.materials) {
            add_usage(
                scene,
                usages,
                material.texture_binding_base_color,
                TextureUsage::Color);
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
                TextureUsage::Scalar);
            add_usage(
                scene,
                usages,
                material.texture_binding_emissive,
                TextureUsage::Color);
        }

        if (scene.environment_light.texture !=
            scene::StaticScene::INVALID_INDEX) {
            if (scene.environment_light.texture >= usages.size()) {
                throw std::runtime_error(
                    "Environment texture index is invalid.");
            }
            usages[scene.environment_light.texture].usage |=
                static_cast<std::uint32_t>(TextureUsage::Environment);
        }

        std::vector<TextureCompressionPlan> result;
        result.reserve(usages.size());
        for (const auto& usage : usages) {
            TextureCompressionPlan plan;
            const auto contains = [&usage](TextureUsage expected) {
                return (usage.usage & static_cast<std::uint32_t>(expected)) != 0;
            };
            const bool environment = contains(TextureUsage::Environment);
            const bool color = contains(TextureUsage::Color);
            const bool normal = contains(TextureUsage::Normal);
            const bool scalar = contains(TextureUsage::Scalar);

            if (environment && !color && !normal && !scalar) {
                plan.dxgi_format = DXGI_FORMAT_BC6H_UF16;
            }
            else if (normal && !color && !scalar && !environment) {
                plan.dxgi_format = DXGI_FORMAT_BC5_UNORM;
            }
            else if (scalar && !color && !normal && !environment &&
                std::ranges::count(usage.scalar_channels, true) == 1) {
                plan.dxgi_format = DXGI_FORMAT_BC4_UNORM;
                plan.source_channel = scalar_channel(usage);
                plan.isolate_source_channel = true;
                plan.linearize_source_channel = usage.srgb;
            }
            else {
                plan.dxgi_format = DXGI_FORMAT_BC7_UNORM;
                plan.filter_as_srgb = usage.srgb;
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
