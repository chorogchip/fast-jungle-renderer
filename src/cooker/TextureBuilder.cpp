#include "TextureBuilder.hpp"

#include <DirectXTex.h>

#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "TextureImageBuilder.hpp"
#include "TexturePayloadBuilder.hpp"
#include "TexturePlanBuilder.hpp"
#include "TextureSourceBuilder.hpp"
#include "FastJungle/core/util/ComInitializer.hpp"
#include "FastJungle/core/util/Logger.hpp"

namespace fjr::cooker {
    scene::StaticTexturePayload TextureBuilder::build(
        scene::StaticScene& scene, const std::filesystem::path& output_path,
        std::span<const GeneratedTexture> generated_textures) {
        auto payload_path = output_path;
        payload_path += L".textures.tmp";

        const util::ComInitializer com_initializer;
        const auto compression_plans = TexturePlanBuilder::build(scene);
        std::unordered_map<std::string_view, const GeneratedTexture*>
            generated_by_key;
        generated_by_key.reserve(generated_textures.size());
        for (const auto& generated : generated_textures) {
            generated_by_key.emplace(generated.key, &generated);
        }
        std::vector<scene::StaticScene::Texture> cooked_textures =
            scene.textures;
        std::vector<scene::StaticScene::TextureMip> cooked_mips;
        std::vector<scene::StaticScene::TextureBinding> cooked_bindings =
            scene.texture_bindings;
        for (auto& binding : cooked_bindings) {
            if (binding.texture >= compression_plans.size()) {
                log::Logger::g_logger << log::abrt(
                    "Texture compression binding index is invalid.");
            }
            binding = normalize_texture_binding(
                binding, compression_plans[binding.texture]);
        }
        TexturePayloadBuilder payload{std::move(payload_path)};
        for (std::size_t index = 0; index < scene.textures.size(); ++index) {
            const auto key = texture_source_key(
                scene,
                static_cast<uint32_t>(index));
            const std::filesystem::path path{key};
            const auto generated = generated_by_key.find(key);
            auto source = generated != generated_by_key.end()
                ? TextureSourceBuilder::build(
                    generated->second->image,
                    key)
                : TextureSourceBuilder::build(path);
            auto plan = compression_plans[index];
            if (generated != generated_by_key.end() &&
                generated->second->uncompressed_output_format !=
                    DXGI_FORMAT_UNKNOWN) {
                plan.dxgi_format = static_cast<uint32_t>(
                    generated->second->uncompressed_output_format);
                plan.filter_as_srgb = false;
                plan.use_block_compression = false;
            }
            DirectX::ScratchImage processed;
            TextureImageBuilder::build(source.image, plan, key, processed);
            payload.append(processed, cooked_textures[index], cooked_mips,
                           path);
        }
        scene.textures = std::move(cooked_textures);
        scene.texture_mips = std::move(cooked_mips);
        scene.texture_bindings = std::move(cooked_bindings);

        return payload.finish();
    }

} // namespace fjr::cooker
