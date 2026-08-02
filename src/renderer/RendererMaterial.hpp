#pragma once

#include "FastJungle/renderer/Renderer.hpp"

#include <array>
#include <wincodec.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fjr {

    struct Renderer::MaterialDescription {
        enum Flag : std::uint32_t {
            BaseColorTexture = 1u << 0u,
            NormalTexture = 1u << 1u,
            RoughnessTexture = 1u << 2u,
            OpacityTexture = 1u << 3u,
        };

        struct TextureBinding {
            std::filesystem::path path;
            std::string source_output;
            bool srgb = false;
            D3D12_TEXTURE_ADDRESS_MODE address_u =
                D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            D3D12_TEXTURE_ADDRESS_MODE address_v =
                D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        };

        struct Constants {
            std::array<float, 4> base_color{1.0f, 1.0f, 1.0f, 1.0f};
            std::array<float, 4> emissive_roughness{0.0f, 0.0f, 0.0f, 0.5f};
            std::array<float, 4> surface{0.0f, 1.0f, 0.0f, 0.0f};
            std::array<std::uint32_t, 4> options{};
        } constants;

        std::string material_path;
        std::string uv_primvar;
        TextureBinding base_color;
        TextureBinding normal;
        TextureBinding roughness;
        TextureBinding opacity;
    };

    class Renderer::MaterialResolver {
    public:
        explicit MaterialResolver(const scene::JungleScene& scene);

        [[nodiscard]] MaterialDescription resolve(
            std::string_view material_path) const;

    private:
        using Scene = scene::JungleScene;

        [[nodiscard]] const Scene::ShaderNode& find_shader(
            const std::string& path) const;
        [[nodiscard]] const Scene::ShaderInput* find_input(
            const Scene::ShaderNode& shader,
            std::string_view name) const noexcept;
        [[nodiscard]] MaterialDescription::TextureBinding resolve_texture(
            const Scene::ShaderInput& input,
            bool default_srgb,
            std::string& uv_primvar) const;

        const Scene& scene_;
        std::unordered_map<std::string, const Scene::Material*> materials_;
        std::unordered_map<std::string, const Scene::ShaderNode*> shaders_;
    };

    class Renderer::TextureLoader {
    public:
        TextureLoader();
        ~TextureLoader();

        TextureLoader(const TextureLoader&) = delete;
        TextureLoader& operator=(const TextureLoader&) = delete;

        void init(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            std::size_t material_capacity);

        [[nodiscard]] std::uint32_t add_material(
            MaterialDescription& material);

        void finish_uploads();

        [[nodiscard]] ID3D12DescriptorHeap* resource_heap() const noexcept;
        [[nodiscard]] ID3D12DescriptorHeap* sampler_heap() const noexcept;
        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE material_handle(
            std::uint32_t material_index) const noexcept;
        [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE sampler_handle(
            std::uint32_t material_index) const noexcept;

        [[nodiscard]] std::uint32_t loaded_texture_count() const noexcept {
            return loaded_texture_count_;
        }

        [[nodiscard]] std::uint32_t fallback_binding_count() const noexcept {
            return fallback_binding_count_;
        }

    private:
        struct DecodedImage;
        struct TextureRecord;

        [[nodiscard]] DecodedImage decode(
            const std::filesystem::path& path) const;
        [[nodiscard]] std::uint32_t create_texture(
            std::wstring cache_key,
            const std::filesystem::path& path,
            const DecodedImage& image);
        [[nodiscard]] std::uint32_t find_or_load(
            const MaterialDescription::TextureBinding& binding,
            std::uint32_t fallback_texture,
            MaterialDescription& material,
            MaterialDescription::Flag flag);
        void create_srv(
            std::uint32_t texture_index,
            std::uint32_t descriptor_index,
            bool srgb) const;
        void create_sampler(
            const MaterialDescription::TextureBinding& binding,
            std::uint32_t descriptor_index) const;

        ID3D12Device* device_ = nullptr;
        ID3D12GraphicsCommandList* command_list_ = nullptr;
        bool uninitialize_com_ = false;
        std::uint32_t material_count_ = 0;
        std::uint32_t loaded_texture_count_ = 0;
        std::uint32_t fallback_binding_count_ = 0;
        std::uint32_t white_texture_ = 0;
        std::uint32_t normal_texture_ = 0;
        std::uint32_t roughness_texture_ = 0;

        dx::DescriptorHeap resource_heap_;
        dx::DescriptorHeap sampler_heap_;
        Microsoft::WRL::ComPtr<IWICImagingFactory> wic_factory_;
        std::vector<std::unique_ptr<TextureRecord>> textures_;
        std::vector<dx::Buffer> upload_buffers_;
        std::unordered_map<std::wstring, std::uint32_t> texture_cache_;
    };

} // namespace fjr
