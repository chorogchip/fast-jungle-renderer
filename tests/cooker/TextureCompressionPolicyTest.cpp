#include <dxgiformat.h>

#include <cstdint>
#include <stdexcept>

#include "../../src/cooker/TextureCompression.hpp"

namespace {

    using Scene = fjr::scene::StaticScene;

    [[nodiscard]] std::uint32_t add_binding(
        Scene& scene,
        std::uint32_t texture,
        Scene::EnumTextureChannel channel,
        Scene::EnumTextureBindingFlag flags) {

        Scene::TextureBinding binding;
        binding.texture = texture;
        binding.sampler = 0;
        binding.channel = channel;
        binding.flags = flags;
        scene.texture_bindings.push_back(binding);
        return static_cast<std::uint32_t>(
            scene.texture_bindings.size() - 1);
    }

    [[nodiscard]] Scene make_scene() {
        Scene scene;
        scene.textures.resize(7);

        Scene::Material color;
        color.texture_binding_base_color = add_binding(
            scene,
            0,
            Scene::EnumTextureChannel::RGB,
            Scene::EnumTextureBindingFlag::SRGB);
        color.texture_binding_opacity = add_binding(
            scene,
            0,
            Scene::EnumTextureChannel::A,
            Scene::EnumTextureBindingFlag::SRGB);
        scene.materials.push_back(color);

        Scene::Material normal;
        normal.texture_binding_normal = add_binding(
            scene,
            1,
            Scene::EnumTextureChannel::RGBA,
            Scene::EnumTextureBindingFlag::LINEAR);
        scene.materials.push_back(normal);

        Scene::Material roughness_green;
        roughness_green.texture_binding_roughness = add_binding(
            scene,
            2,
            Scene::EnumTextureChannel::G,
            Scene::EnumTextureBindingFlag::SRGB);
        scene.materials.push_back(roughness_green);

        Scene::Material opacity;
        opacity.texture_binding_opacity = add_binding(
            scene,
            3,
            Scene::EnumTextureChannel::R,
            Scene::EnumTextureBindingFlag::SRGB);
        scene.materials.push_back(opacity);

        scene.environment_light.texture = 4;

        Scene::Material mixed_red;
        mixed_red.texture_binding_roughness = add_binding(
            scene,
            5,
            Scene::EnumTextureChannel::R,
            Scene::EnumTextureBindingFlag::LINEAR);
        scene.materials.push_back(mixed_red);

        Scene::Material mixed_green;
        mixed_green.texture_binding_roughness = add_binding(
            scene,
            5,
            Scene::EnumTextureChannel::G,
            Scene::EnumTextureBindingFlag::LINEAR);
        scene.materials.push_back(mixed_green);

		// The Jungle terrain packs roughness in G and metallic in B. Both
		// consumers must keep the shared texture multi-channel instead of
		// destructively isolating whichever scalar happens to be visited first.
		Scene::Material packed_roughness;
		packed_roughness.texture_binding_roughness = add_binding(
			scene,
			6,
			Scene::EnumTextureChannel::G,
			Scene::EnumTextureBindingFlag::LINEAR);
		scene.materials.push_back(packed_roughness);

		Scene::Material packed_metallic;
		packed_metallic.texture_binding_metallic = add_binding(
			scene,
			6,
			Scene::EnumTextureChannel::B,
			Scene::EnumTextureBindingFlag::LINEAR);
		scene.materials.push_back(packed_metallic);
        return scene;
    }

} // namespace

int main() {
    auto scene = make_scene();
    const auto plans =
        fjr::cooker::resolve_texture_compression(scene);
    if (plans.size() != scene.textures.size() ||
        plans[0].dxgi_format != DXGI_FORMAT_BC7_UNORM ||
        !plans[0].filter_as_srgb ||
        plans[1].dxgi_format != DXGI_FORMAT_BC5_UNORM ||
        plans[2].dxgi_format != DXGI_FORMAT_BC4_UNORM ||
        plans[2].source_channel != Scene::EnumTextureChannel::G ||
        !plans[2].isolate_source_channel ||
        !plans[2].linearize_source_channel ||
        plans[3].dxgi_format != DXGI_FORMAT_BC4_UNORM ||
        plans[4].dxgi_format != DXGI_FORMAT_BC6H_UF16 ||
        plans[5].dxgi_format != DXGI_FORMAT_BC7_UNORM ||
		plans[5].isolate_source_channel ||
		plans[6].dxgi_format != DXGI_FORMAT_BC7_UNORM ||
		plans[6].isolate_source_channel) {
        throw std::runtime_error(
            "Texture compression policy selection failed.");
    }

    for (auto& binding : scene.texture_bindings)
        binding = fjr::cooker::normalize_texture_binding(
            binding, plans[binding.texture]);
    if (scene.texture_bindings[1].channel !=
            Scene::EnumTextureChannel::A ||
        scene.texture_bindings[3].channel !=
            Scene::EnumTextureChannel::R ||
        scene.texture_bindings[3].flags !=
            Scene::EnumTextureBindingFlag::LINEAR ||
        scene.texture_bindings[6].channel !=
			Scene::EnumTextureChannel::G ||
		scene.texture_bindings[7].channel !=
			Scene::EnumTextureChannel::G ||
		scene.texture_bindings[8].channel !=
			Scene::EnumTextureChannel::B) {
        throw std::runtime_error(
            "Texture compression binding normalization failed.");
    }
    return 0;
}
