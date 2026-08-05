#include "FastJungle/cooker/TextureCompressionPolicy.hpp"

#include <dxgiformat.h>

#include <array>
#include <stdexcept>

namespace fjr::cooker {

    namespace {

        enum class TextureUsage : std::uint32_t {
            None = 0,
            Color = 1u << 0u,
            Normal = 1u << 1u,
            Scalar = 1u << 2u,
            Environment = 1u << 3u
        };

        [[nodiscard]] TextureUsage operator|(
            TextureUsage left,
            TextureUsage right) noexcept {

            return static_cast<TextureUsage>(
                static_cast<std::uint32_t>(left) |
                static_cast<std::uint32_t>(right));
        }

        [[nodiscard]] bool contains(
            TextureUsage value,
            TextureUsage expected) noexcept {

            return (static_cast<std::uint32_t>(value) &
                static_cast<std::uint32_t>(expected)) != 0;
        }

        struct UsageRecord final {
            TextureUsage usage = TextureUsage::None;
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

            auto& record = usages[binding.texture];
            record.usage = record.usage | usage;
            record.srgb = record.srgb ||
                binding.flags ==
                    scene::StaticScene::EnumTextureBindingFlag::SRGB;
            if (usage == TextureUsage::Scalar) {
                record.scalar_channels[channel_index(binding.channel)] = true;
            }
        }

        [[nodiscard]] std::size_t count_scalar_channels(
            const UsageRecord& usage) noexcept {

            std::size_t count = 0;
            for (const bool used : usage.scalar_channels) {
                count += used ? 1u : 0u;
            }
            return count;
        }

        [[nodiscard]] scene::StaticScene::EnumTextureChannel scalar_channel(
            const UsageRecord& usage) noexcept {

            for (std::size_t index = 0;
                 index < usage.scalar_channels.size();
                 ++index) {
                if (usage.scalar_channels[index]) {
                    return static_cast<
                        scene::StaticScene::EnumTextureChannel>(index);
                }
            }
            return scene::StaticScene::EnumTextureChannel::R;
        }

    } // namespace

    std::vector<TextureCompressionPlan>
    TextureCompressionPolicy::resolve(const scene::StaticScene& scene) {
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
            usages[scene.environment_light.texture].usage =
                usages[scene.environment_light.texture].usage |
                TextureUsage::Environment;
        }

        std::vector<TextureCompressionPlan> result;
        result.reserve(usages.size());
        for (const auto& usage : usages) {
            TextureCompressionPlan plan;
            const bool environment = contains(
                usage.usage,
                TextureUsage::Environment);
            const bool color = contains(usage.usage, TextureUsage::Color);
            const bool normal = contains(usage.usage, TextureUsage::Normal);
            const bool scalar = contains(usage.usage, TextureUsage::Scalar);

            if (environment && !color && !normal && !scalar) {
                plan.dxgi_format = DXGI_FORMAT_BC6H_UF16;
            }
            else if (normal && !color && !scalar && !environment) {
                plan.dxgi_format = DXGI_FORMAT_BC5_UNORM;
            }
            else if (scalar && !color && !normal && !environment &&
                count_scalar_channels(usage) == 1) {
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

    void TextureCompressionPolicy::apply_binding_changes(
        scene::StaticScene& scene,
        std::span<const TextureCompressionPlan> plans) {

        if (plans.size() != scene.textures.size()) {
            throw std::invalid_argument(
                "Texture compression plan count is invalid.");
        }

        for (auto& binding : scene.texture_bindings) {
            if (binding.texture >= plans.size()) {
                throw std::runtime_error(
                    "Texture compression binding index is invalid.");
            }
            if (!plans[binding.texture].isolate_source_channel) {
                continue;
            }
            binding.channel = scene::StaticScene::EnumTextureChannel::R;
            binding.flags =
                scene::StaticScene::EnumTextureBindingFlag::LINEAR;
        }
    }

} // namespace fjr::cooker
