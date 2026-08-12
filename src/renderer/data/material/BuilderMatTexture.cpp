#include "FastJungle/renderer/data/material/BuilderMatTexture.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/FormatUtils.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render::data::mat {

    namespace {

        [[nodiscard]]
        UINT get_mip_count(
            const scene::StaticScene::Texture& texture) {

            if (texture.mip_count == 0 ||
                texture.mip_count > std::numeric_limits<UINT16>::max()) {

                log::Logger::g_logger << log::abrt(
                    "Scene texture mip count is unsupported by D3D12.");
            }

            return texture.mip_count;
        }

        [[nodiscard]]
        D3D12_RESOURCE_DESC make_texture_description(
            const scene::StaticScene::Texture& texture) {

            D3D12_RESOURCE_DESC description{};

            description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            description.Width = texture.width;
            description.Height = texture.height;
            description.DepthOrArraySize = 1;
            description.MipLevels = static_cast<UINT16>(
                get_mip_count(texture));
            description.Format = dx::FormatUtils::to_bc(
                static_cast<DXGI_FORMAT>(texture.dxgi_format));
            description.SampleDesc.Count = 1;
            description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            description.Flags = D3D12_RESOURCE_FLAG_NONE;

            return description;
        }

        [[nodiscard]]
        uint32_t get_texture_id(
            const scene::StaticScene& scene,
            uint32_t binding_id) {

            if (binding_id == scene::StaticScene::INVALID_INDEX)
                return Consts::IND_ERR;

            if (binding_id >= scene.texture_bindings.size()) {
                log::Logger::g_logger << log::abrt(
                    "Material contains an invalid texture binding index.");
            }

            const auto texture_id =
                scene.texture_bindings[binding_id].texture;

            if (texture_id == scene::StaticScene::INVALID_INDEX ||
                texture_id >= scene.textures.size()) {

                log::Logger::g_logger << log::abrt(
                    "Texture binding contains an invalid texture index.");
            }

            return texture_id;
        }

        [[nodiscard]]
        std::vector<bool> build_srgb_table(
            const scene::StaticScene& scene) {

            std::vector<bool> result(scene.textures.size(), false);

            for (const auto& material : scene.materials) {
                if (material.texture_binding_base_color ==
                    scene::StaticScene::INVALID_INDEX) {

                    continue;
                }

                const auto texture_id = get_texture_id(
                    scene,
                    material.texture_binding_base_color);

                result[texture_id] = true;
            }

            return result;
        }

        void create_textures(
            DataPersistent& output,
            dx::ResourceUploader& uploader,
            ID3D12Device* device,
            const scene::StaticScene& scene) {

            output.textures.resize(scene.textures.size());

            std::vector<dx::TextureSubresourceData> subresources;

            for (std::size_t texture_id = 0;
                texture_id < scene.textures.size();
                ++texture_id) {

                const auto& source_texture = scene.textures[texture_id];
                const UINT mip_count = get_mip_count(source_texture);
                const auto description =
                    make_texture_description(source_texture);
                auto& texture = output.textures[texture_id];

                texture.init(
                    device,
                    description,
                    D3D12_RESOURCE_STATE_COMMON);

                subresources.resize(mip_count);

                for (uint32_t mip_id = 0;
                    mip_id < mip_count;
                    ++mip_id) {

                    const std::size_t source_mip_id =
                        static_cast<std::size_t>(source_texture.mip_offset) +
                        mip_id;

                    if (source_mip_id >= scene.texture_mips.size()) {
                        log::Logger::g_logger << log::abrt(
                            "Texture mip range is invalid.");
                    }

                    const auto& source_mip =
                        scene.texture_mips[source_mip_id];

                    const UINT64 byte_offset =
                        source_texture.data_byte_offset +
                        source_mip.data_byte_offset_local;

                    if (byte_offset > scene.texture_data.size() ||
                        source_mip.slice_pitch >
                        scene.texture_data.size() - byte_offset) {

                        log::Logger::g_logger << log::abrt(
                            "Texture payload range is invalid.");
                    }

                    subresources[mip_id] = {
                        std::span<const std::byte>{
                            scene.texture_data.data() + byte_offset,
                            static_cast<std::size_t>(
                                source_mip.slice_pitch),
                        },
                        source_mip.row_pitch,
                        source_mip.slice_pitch,
                    };
                }

                uploader.upload_texture(
                    texture,
                    subresources,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            }
        }

        void create_texture_descriptors(
            DataPersistent& output,
            ID3D12Device* device,
            const scene::StaticScene& scene) {

            const auto srgb = build_srgb_table(scene);

            if (scene.textures.empty()) {
                D3D12_SHADER_RESOURCE_VIEW_DESC description{};
                description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                description.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                description.Shader4ComponentMapping =
                    D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                description.Texture2D.MipLevels = 1;

                device->CreateShaderResourceView(
                    nullptr,
                    &description,
                    output.texture_descriptors.get_cpu());

                return;
            }

            for (std::size_t texture_id = 0;
                texture_id < scene.textures.size();
                ++texture_id) {

                const auto& source = scene.textures[texture_id];
                const auto source_format =
                    static_cast<DXGI_FORMAT>(source.dxgi_format);
                const DXGI_FORMAT view_format = srgb[texture_id]
                    ? dx::FormatUtils::to_srgb(source_format)
                    : dx::FormatUtils::to_linear(source_format);

                output.textures[texture_id].create_srv(
                    device,
                    output.texture_descriptors.get_cpu(
                        static_cast<UINT>(texture_id)),
                    dx::TextureViewRange{
                        0,
                        get_mip_count(source),
                        0,
                        1,
                    },
                    view_format);
            }
        }

    } // namespace

    void BuilderMatTexture::build(
        DataPersistent& output,
        dx::ResourceUploader& uploader,
        ID3D12Device* device,
        dx::DescriptorHeap& heap_srv_cbv_uav,
        const scene::StaticScene& scene) {

        output.texture_descriptors = heap_srv_cbv_uav.alloc(
            static_cast<UINT>(scene.textures.size()));

        create_textures(output, uploader, device, scene);
        create_texture_descriptors(output, device, scene);
    }

} // namespace fjr::render::data::mat
