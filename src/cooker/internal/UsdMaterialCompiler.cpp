#include "UsdMaterialCompiler.hpp"

#include "CookError.hpp"
#include "PathKey.hpp"
#include "StaticSceneAssembler.hpp"

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/path.h>

#include <cstdint>
#include <utility>

namespace fjr::cooker::internal {

    using StaticScene = scene::StaticScene;

    StaticScene::EnumSubmeshFlag MaterialProduct::submesh_flags(
        bool double_sided) const noexcept {

        std::uint32_t flags = double_sided
            ? static_cast<std::uint32_t>(
                StaticScene::EnumSubmeshFlag::DOUBLE_SIDED)
            : 0u;
        if (alpha_tested) {
            flags |= static_cast<std::uint32_t>(
                StaticScene::EnumSubmeshFlag::ALPHA_TESTED);
        }
        return static_cast<StaticScene::EnumSubmeshFlag>(flags);
    }

    UsdMaterialCompiler::UsdMaterialCompiler(
        StaticSceneAssembler& assembler)
        : assembler_(assembler) {}

    MaterialProduct UsdMaterialCompiler::compile(
        const std::string& path,
        const pxr::UsdShadeMaterial& source) {

        const std::string key = path.empty() ? "__fallback__" : path;
        const auto cached = cache_.find(key);
        if (cached != cache_.end()) {
            return cached->second;
        }

        StaticScene::Material material;
        material.name = assembler_.intern_string(
            path.empty() ? "DefaultMaterial" : path_leaf(path)).value();
        MaterialProduct product;

        if (source) {
            const auto surface = find_surface_shader(source);
            material.base_color = read_float3_as_color(
                surface,
                "diffuseColor",
                {0.18f, 0.18f, 0.18f});
            material.emissive = read_float3(
                surface,
                "emissiveColor",
                {0.0f, 0.0f, 0.0f});
            material.roughness = read_float(surface, "roughness", 0.5f);
            material.metallic = read_float(surface, "metallic", 0.0f);
            material.opacity = read_float(surface, "opacity", 1.0f);
            material.opacity_threshold = read_float(
                surface,
                "opacityThreshold",
                0.0f);
            material.ior = read_float(surface, "ior", 1.5f);
            material.specular = read_float(surface, "specular", 0.5f);
            material.clearcoat = read_float(surface, "clearcoat", 0.0f);
            material.clearcoat_roughness = read_float(
                surface,
                "clearcoatRoughness",
                0.01f);

            std::string uv_primvar;
            if (resolve_material_texture(
                surface,
                "diffuseColor",
                true,
                material.texture_binding_base_color,
                uv_primvar)) {
                material.base_color = {1.0f, 1.0f, 1.0f, 1.0f};
            }
            static_cast<void>(resolve_material_texture(
                surface,
                "normal",
                false,
                material.texture_binding_normal,
                uv_primvar));
            static_cast<void>(resolve_material_texture(
                surface,
                "roughness",
                false,
                material.texture_binding_roughness,
                uv_primvar));
            static_cast<void>(resolve_material_texture(
                surface,
                "metallic",
                false,
                material.texture_binding_metallic,
                uv_primvar));
            static_cast<void>(resolve_material_texture(
                surface,
                "opacity",
                false,
                material.texture_binding_opacity,
                uv_primvar));
            if (resolve_material_texture(
                surface,
                "emissiveColor",
                true,
                material.texture_binding_emissive,
                uv_primvar)) {
                material.emissive = {1.0f, 1.0f, 1.0f};
            }

            product.uv_primvar = uv_primvar.empty()
                ? "st"
                : std::move(uv_primvar);
            product.has_textures =
                material.texture_binding_base_color !=
                    StaticScene::INVALID_INDEX ||
                material.texture_binding_normal != StaticScene::INVALID_INDEX ||
                material.texture_binding_roughness != StaticScene::INVALID_INDEX ||
                material.texture_binding_metallic != StaticScene::INVALID_INDEX ||
                material.texture_binding_opacity != StaticScene::INVALID_INDEX ||
                material.texture_binding_emissive != StaticScene::INVALID_INDEX;

            if (material.opacity_threshold > 0.0f) {
                product.alpha_tested = true;
            }
        }

        product.id = assembler_.append_material(material);
        cache_.emplace(key, product);
        return product;
    }

    pxr::UsdShadeShader UsdMaterialCompiler::find_surface_shader(
        const pxr::UsdShadeMaterial& material) const {

        for (const auto& output : material.GetOutputs()) {
            if (output.GetBaseName() != pxr::TfToken{"surface"}) {
                continue;
            }
            pxr::SdfPathVector invalid_paths;
            const auto sources = output.GetConnectedSources(&invalid_paths);
            if (sources.empty()) {
                continue;
            }
            if (sources.size() != 1 || !invalid_paths.empty()) {
                fail(
                    "Material surface must have one valid shader source: ",
                    material.GetPath().GetString());
            }
            const pxr::UsdShadeShader shader{
                sources.front().source.GetPrim()};
            pxr::TfToken shader_id;
            if (shader && shader.GetShaderId(&shader_id) &&
                shader_id == pxr::TfToken{"UsdPreviewSurface"}) {
                return shader;
            }
        }
        fail(
            "Material has no UsdPreviewSurface: ",
            material.GetPath().GetString());
    }

    pxr::UsdShadeInput UsdMaterialCompiler::find_input(
        const pxr::UsdShadeShader& shader,
        std::string_view name) {

        return shader.GetInput(pxr::TfToken{std::string{name}});
    }

    float UsdMaterialCompiler::read_float(
        const pxr::UsdShadeShader& shader,
        std::string_view name,
        float fallback) {

        const auto input = find_input(shader, name);
        float result = fallback;
        if (input) {
            input.Get(&result);
        }
        return result;
    }

    DirectX::XMFLOAT3 UsdMaterialCompiler::read_float3(
        const pxr::UsdShadeShader& shader,
        std::string_view name,
        const DirectX::XMFLOAT3& fallback) {

        const auto input = find_input(shader, name);
        pxr::GfVec3f result{fallback.x, fallback.y, fallback.z};
        if (input) {
            input.Get(&result);
        }
        return {result[0], result[1], result[2]};
    }

    DirectX::XMFLOAT4 UsdMaterialCompiler::read_float3_as_color(
        const pxr::UsdShadeShader& shader,
        std::string_view name,
        const DirectX::XMFLOAT3& fallback) {

        const auto value = read_float3(shader, name, fallback);
        return {value.x, value.y, value.z, 1.0f};
    }

    bool UsdMaterialCompiler::resolve_material_texture(
        const pxr::UsdShadeShader& surface,
        std::string_view input_name,
        bool default_srgb,
        std::uint32_t& destination,
        std::string& uv_primvar) {

        const auto input = find_input(surface, input_name);
        if (!input) {
            return false;
        }

        pxr::SdfPathVector invalid_paths;
        const auto sources = input.GetConnectedSources(&invalid_paths);
        if (sources.empty()) {
            return false;
        }
        if (sources.size() != 1 || !invalid_paths.empty()) {
            fail(
                "Material input must have one valid texture source: ",
                surface.GetPath().GetString(),
                ".",
                std::string{input_name});
        }

        const auto sample = resolve_texture_sample(
            sources.front(),
            default_srgb,
            input_name == "normal");
        if (uv_primvar.empty()) {
            uv_primvar = sample.uv_primvar;
        }
        else if (uv_primvar != sample.uv_primvar) {
            fail(
                "Material uses multiple UV primvars: ",
                surface.GetPath().GetString());
        }
        destination = commit_texture_sample(sample).value();
        return true;
    }

    TextureSampleSemantic UsdMaterialCompiler::resolve_texture_sample(
        const pxr::UsdShadeConnectionSourceInfo& connection,
        bool default_srgb,
        bool normal_input) const {

        const pxr::UsdShadeShader texture_shader{
            connection.source.GetPrim()};
        pxr::TfToken shader_id;
        if (!texture_shader ||
            !texture_shader.GetShaderId(&shader_id) ||
            shader_id != pxr::TfToken{"UsdUVTexture"}) {
            fail("Connected shader is not UsdUVTexture.");
        }

        const auto file_input = texture_shader.GetInput(pxr::TfToken{"file"});
        pxr::SdfAssetPath asset;
        if (!file_input || !file_input.Get(&asset)) {
            fail(
                "UsdUVTexture has no file: ",
                texture_shader.GetPath().GetString());
        }
        const std::string resolved_path = asset.GetResolvedPath();
        if (resolved_path.empty()) {
            fail("Texture asset is unresolved: ", asset.GetAssetPath());
        }

        TextureSampleSemantic result;
        result.asset = std::filesystem::path{resolved_path};
        result.components = texture_components(
            connection.sourceName.GetString());
        result.color_space = default_srgb
            ? StaticScene::EnumTextureBindingFlag::SRGB
            : StaticScene::EnumTextureBindingFlag::LINEAR;

        const auto color_space = read_token_or_string(
            texture_shader.GetInput(pxr::TfToken{"sourceColorSpace"}));
        if (color_space == "sRGB") {
            result.color_space = StaticScene::EnumTextureBindingFlag::SRGB;
        }
        else if (color_space == "raw") {
            result.color_space = StaticScene::EnumTextureBindingFlag::LINEAR;
        }

        result.sampler.address_u = texture_address_mode(
            read_token_or_string(
                texture_shader.GetInput(pxr::TfToken{"wrapS"})));
        result.sampler.address_v = texture_address_mode(
            read_token_or_string(
                texture_shader.GetInput(pxr::TfToken{"wrapT"})));

        pxr::GfVec4f scale{1.0f};
        pxr::GfVec4f bias{0.0f};
        if (const auto input = texture_shader.GetInput(pxr::TfToken{"scale"})) {
            input.Get(&scale);
        }
        if (const auto input = texture_shader.GetInput(pxr::TfToken{"bias"})) {
            input.Get(&bias);
        }
        const pxr::GfVec4f expected_scale = normal_input
            ? pxr::GfVec4f{2.0f}
            : pxr::GfVec4f{1.0f};
        const pxr::GfVec4f expected_bias = normal_input
            ? pxr::GfVec4f{-1.0f}
            : pxr::GfVec4f{0.0f};
        if (scale != expected_scale || bias != expected_bias) {
            fail(
                "Unsupported UsdUVTexture scale/bias: ",
                texture_shader.GetPath().GetString());
        }

        const auto st = texture_shader.GetInput(pxr::TfToken{"st"});
        if (st) {
            pxr::SdfPathVector invalid_paths;
            const auto st_sources = st.GetConnectedSources(&invalid_paths);
            if (!st_sources.empty()) {
                if (st_sources.size() != 1 || !invalid_paths.empty()) {
                    fail(
                        "Texture st must have one valid primvar source: ",
                        texture_shader.GetPath().GetString());
                }
                const pxr::UsdShadeShader reader{
                    st_sources.front().source.GetPrim()};
                pxr::TfToken reader_id;
                if (!reader ||
                    !reader.GetShaderId(&reader_id) ||
                    reader_id != pxr::TfToken{"UsdPrimvarReader_float2"}) {
                    fail("Texture st source is not a float2 primvar reader.");
                }
                const auto varname = read_token_or_string(
                    reader.GetInput(pxr::TfToken{"varname"}));
                if (!varname.empty()) {
                    result.uv_primvar = varname;
                }
            }
        }
        return result;
    }

    TextureBindingId UsdMaterialCompiler::commit_texture_sample(
        const TextureSampleSemantic& sample) {

        StaticScene::TextureBinding binding;
        binding.texture = assembler_.intern_texture(sample.asset).value();
        binding.sampler = assembler_.intern_sampler(sample.sampler).value();
        binding.channel = sample.components;
        binding.flags = sample.color_space;
        return assembler_.append_texture_binding(binding);
    }

    std::string UsdMaterialCompiler::read_token_or_string(
        const pxr::UsdShadeInput& input) {

        if (!input) {
            return {};
        }
        pxr::VtValue value;
        if (!input.Get(&value) || value.IsEmpty()) {
            return {};
        }
        if (value.IsHolding<pxr::TfToken>()) {
            return value.UncheckedGet<pxr::TfToken>().GetString();
        }
        if (value.IsHolding<std::string>()) {
            return value.UncheckedGet<std::string>();
        }
        return {};
    }

    StaticScene::EnumSamplerAddressMode
    UsdMaterialCompiler::texture_address_mode(std::string_view value) {

        if (value.empty() || value == "repeat" || value == "useMetadata") {
            return StaticScene::EnumSamplerAddressMode::WRAP;
        }
        if (value == "mirror") {
            return StaticScene::EnumSamplerAddressMode::MIRROR;
        }
        if (value == "clamp") {
            return StaticScene::EnumSamplerAddressMode::CLAMP;
        }
        if (value == "black") {
            return StaticScene::EnumSamplerAddressMode::BORDER;
        }
        fail("Unsupported texture address mode: ", std::string{value});
    }

    StaticScene::EnumTextureChannel
    UsdMaterialCompiler::texture_components(
        std::string_view output) noexcept {

        if (output == "r") return StaticScene::EnumTextureChannel::R;
        if (output == "g") return StaticScene::EnumTextureChannel::G;
        if (output == "b") return StaticScene::EnumTextureChannel::B;
        if (output == "a") return StaticScene::EnumTextureChannel::A;
        if (output == "rgb") return StaticScene::EnumTextureChannel::RGB;
        return StaticScene::EnumTextureChannel::RGBA;
    }

} // namespace fjr::cooker::internal
