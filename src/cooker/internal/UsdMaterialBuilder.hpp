#pragma once

#include "SceneHandles.hpp"

#include "FastJungle/scene/StaticScene.hpp"

#include <pxr/usd/usdShade/connectableAPI.h>
#include <pxr/usd/usdShade/input.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/shader.h>

#include <DirectXMath.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace fjr::cooker::internal {

    class StaticSceneDataBuilder;

    struct MaterialProduct final {
        MaterialId id;
        std::string uv_primvar = "st";
        bool has_textures = false;
        bool alpha_tested = false;

        [[nodiscard]] scene::StaticScene::EnumSubmeshFlag submesh_flags(
            bool double_sided) const noexcept;
    };

    struct TextureSampleSemantic final {
        std::filesystem::path asset;
        std::string uv_primvar = "st";
        scene::StaticScene::EnumTextureChannel components =
            scene::StaticScene::EnumTextureChannel::RGBA;
        scene::StaticScene::EnumTextureBindingFlag color_space =
            scene::StaticScene::EnumTextureBindingFlag::LINEAR;
        scene::StaticScene::Sampler sampler;
    };

    class UsdMaterialBuilder final {
    public:
        explicit UsdMaterialBuilder(StaticSceneDataBuilder& scene_builder);

        [[nodiscard]] MaterialProduct build(
            const std::string& path,
            const pxr::UsdShadeMaterial& source);

    private:
        [[nodiscard]] pxr::UsdShadeShader find_surface_shader(
            const pxr::UsdShadeMaterial& material) const;

        [[nodiscard]] static pxr::UsdShadeInput find_input(
            const pxr::UsdShadeShader& shader,
            std::string_view name);

        [[nodiscard]] static float read_float(
            const pxr::UsdShadeShader& shader,
            std::string_view name,
            float fallback);

        [[nodiscard]] static DirectX::XMFLOAT3 read_float3(
            const pxr::UsdShadeShader& shader,
            std::string_view name,
            const DirectX::XMFLOAT3& fallback);

        [[nodiscard]] static DirectX::XMFLOAT4 read_float3_as_color(
            const pxr::UsdShadeShader& shader,
            std::string_view name,
            const DirectX::XMFLOAT3& fallback);

        [[nodiscard]] bool resolve_material_texture(
            const pxr::UsdShadeShader& surface,
            std::string_view input_name,
            bool default_srgb,
            uint32_t& destination,
            std::string& uv_primvar);

        [[nodiscard]] TextureSampleSemantic resolve_texture_sample(
            const pxr::UsdShadeConnectionSourceInfo& connection,
            bool default_srgb,
            bool normal_input) const;

        [[nodiscard]] TextureBindingId commit_texture_sample(
            const TextureSampleSemantic& sample);

        [[nodiscard]] static std::string read_token_or_string(
            const pxr::UsdShadeInput& input);

        [[nodiscard]] static scene::StaticScene::EnumSamplerAddressMode
        texture_address_mode(std::string_view value);

        [[nodiscard]] static scene::StaticScene::EnumTextureChannel
        texture_components(std::string_view output) noexcept;

        StaticSceneDataBuilder& scene_builder_;
        std::unordered_map<std::string, MaterialProduct> cache_;
    };

} // namespace fjr::cooker::internal
