#include "FastJungle/renderer/builder/SceneTextureResources.hpp"

#include "FastJungle/dx12/FormatUtils.hpp"
#include "FastJungle/dx12/HeapManager.hpp"
#include "FastJungle/dx12/ResourceUploader.hpp"
#include "FastJungle/dx12/SamplerUtils.hpp"
#include "FastJungle/renderer/SceneResources.hpp"
#include "FastJungle/scene/StaticScene.hpp"

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace fjr::render {

    namespace {

        void create_textures(
            SceneResources& resources,
            dx::ResourceUploader& uploader,
            ID3D12Device* device,
            const scene::StaticScene& scene) {

            if (scene.textures.size() >
                std::numeric_limits<UINT>::max() / 2u) {
                throw std::overflow_error(
                    "Scene texture descriptor count exceeds D3D12 limits.");
            }

            const UINT descriptor_count = std::max(
                1u,
                static_cast<UINT>(scene.textures.size()) * 2u);
            resources.texture_descriptors =
                dx::HeapManager::g_heap_manager.heap_srv_cbv_uav
                    .alloc(descriptor_count);

            if (scene.textures.empty()) {
                D3D12_SHADER_RESOURCE_VIEW_DESC description{};
                description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                description.ViewDimension =
                    D3D12_SRV_DIMENSION_TEXTURE2D;
                description.Shader4ComponentMapping =
                    D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                description.Texture2D.MipLevels = 1;
                device->CreateShaderResourceView(
                    nullptr,
                    &description,
                    resources.texture_descriptors.get_cpu());
                return;
            }

            resources.textures.resize(scene.textures.size());
            std::vector<dx::TextureSubresourceData> subresources;

            for (std::size_t texture_index = 0;
                texture_index < scene.textures.size();
                ++texture_index) {
                const auto& source_texture = scene.textures[texture_index];
                if (source_texture.mip_count == 0 ||
                    source_texture.mip_count >
                        std::numeric_limits<UINT16>::max()) {
                    throw std::invalid_argument(
                        "Scene texture mip count is unsupported by D3D12.");
                }

                const UINT mip_count = source_texture.mip_count;
                const auto source_format = static_cast<DXGI_FORMAT>(
                    source_texture.dxgi_format);

                D3D12_RESOURCE_DESC description{};
                description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                description.Width = source_texture.width;
                description.Height = source_texture.height;
                description.DepthOrArraySize = 1;
                description.MipLevels = static_cast<UINT16>(mip_count);
                description.Format = dx::FormatUtils::to_bc(source_format);
                description.SampleDesc.Count = 1;
                description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
                description.Flags = D3D12_RESOURCE_FLAG_NONE;

                auto& texture = resources.textures[texture_index];
                texture.init(
                    device,
                    description,
                    dx::TextureType::texture2d,
                    D3D12_RESOURCE_STATE_COPY_DEST);

                subresources.clear();
                subresources.reserve(mip_count);
                for (std::uint32_t mip_index = 0;
                    mip_index < mip_count;
                    ++mip_index) {
                    const auto& source_mip = scene.texture_mips[
                        static_cast<std::size_t>(source_texture.mip_offset) +
                        mip_index];
                    const auto* source = scene.texture_data.data() +
                        source_texture.data_byte_offset +
                        source_mip.data_byte_offset_local;
                    subresources.push_back({
                        reinterpret_cast<const std::byte*>(source),
                        source_mip.row_pitch,
                        source_mip.slice_pitch,
                    });
                }

                uploader.upload_texture(
                    texture,
                    subresources,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

                texture.create_srv(
                    device,
                    resources.texture_descriptors.get_cpu(
                        static_cast<UINT>(texture_index) * 2u),
                    dx::TextureViewRange{0, mip_count, 0, 1},
                    dx::FormatUtils::to_linear(source_format));
                texture.create_srv(
                    device,
                    resources.texture_descriptors.get_cpu(
                        static_cast<UINT>(texture_index) * 2u + 1u),
                    dx::TextureViewRange{0, mip_count, 0, 1},
                    dx::FormatUtils::to_srgb(source_format));
            }
        }

        void create_samplers(
            SceneResources& resources,
            ID3D12Device* device,
            const scene::StaticScene& scene) {

            if (scene.samplers.size() >
                std::numeric_limits<UINT>::max()) {
                throw std::overflow_error(
                    "Scene sampler count exceeds D3D12 limits.");
            }

            const UINT sampler_count = std::max(
                1u,
                static_cast<UINT>(scene.samplers.size()));
            resources.sampler_descriptors =
                dx::HeapManager::g_heap_manager.heap_sampler
                    .alloc(sampler_count);

            for (UINT sampler_index = 0;
                sampler_index < sampler_count;
                ++sampler_index) {
                D3D12_SAMPLER_DESC description{};
                description.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                description.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                description.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                description.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                description.MaxAnisotropy = 1;
                description.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
                description.MinLOD = 0.0f;
                description.MaxLOD = FLT_MAX;

                if (sampler_index < scene.samplers.size()) {
                    const auto& source = scene.samplers[sampler_index];
                    description.Filter = static_cast<D3D12_FILTER>(
                        source.filter);
                    description.AddressU =
                        static_cast<D3D12_TEXTURE_ADDRESS_MODE>(
                            source.address_u);
                    description.AddressV =
                        static_cast<D3D12_TEXTURE_ADDRESS_MODE>(
                            source.address_v);
                    description.AddressW =
                        static_cast<D3D12_TEXTURE_ADDRESS_MODE>(
                            source.address_w);
                    description.MaxAnisotropy = std::clamp(
                        source.max_anisotropy,
                        1u,
                        16u);
                }

                dx::SamplerUtils::create(
                    device,
                    resources.sampler_descriptors.get_cpu(sampler_index),
                    description);
            }
        }

    } // namespace

    void create_scene_texture_resources(
        SceneResources& resources,
        dx::ResourceUploader& uploader,
        ID3D12Device* device,
        const scene::StaticScene& scene) {

        create_textures(resources, uploader, device, scene);
        create_samplers(resources, device, scene);
    }

} // namespace fjr::render
