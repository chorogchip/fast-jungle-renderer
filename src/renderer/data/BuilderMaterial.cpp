#include "FastJungle/renderer/data/BuilderMaterial.hpp"

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/FormatUtils.hpp"
#include "FastJungle/dx12/SamplerUtils.hpp"

namespace fjr::render::data {

    namespace {

        constexpr UINT MATERIAL_SAMPLER_COUNT = 2;


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

            description.Dimension =
                D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            description.Width = texture.width;
            description.Height = texture.height;
            description.DepthOrArraySize = 1;
            description.MipLevels =
                static_cast<UINT16>(get_mip_count(texture));
            description.Format = dx::FormatUtils::to_bc(
                static_cast<DXGI_FORMAT>(texture.dxgi_format));
            description.SampleDesc.Count = 1;
            description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            description.Flags = D3D12_RESOURCE_FLAG_NONE;

            return description;
        }


        template <typename T>
        void upload_buffer(
            dx::Buffer& output,
            dx::ResourceUploader& uploader,
            ID3D12Device* device,
            std::span<const T> source,
            D3D12_RESOURCE_STATES final_state) {

            if (source.empty()) {
                return;
            }

            output.init(
                device,
                static_cast<UINT64>(source.size_bytes()),
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_FLAG_NONE,
                D3D12_RESOURCE_STATE_COMMON);

            uploader.upload_buffer(
                output,
                std::as_bytes(source),
                final_state);
        }


        [[nodiscard]]
        std::uint32_t get_texture_id(
            const scene::StaticScene& scene,
            std::uint32_t binding_id) {

            if (binding_id == scene::StaticScene::INVALID_INDEX) {
                return data::Consts::IND_ERR;
            }

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

            std::vector<bool> result(
                scene.textures.size(),
                false);

            for (const auto& material : scene.materials) {

                if (material.texture_binding_base_color ==
                    scene::StaticScene::INVALID_INDEX) {

                    continue;
                }

                const auto texture_id =
                    get_texture_id(
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
            std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints;
            std::vector<UINT> row_counts;
            std::vector<UINT64> row_sizes;

            for (std::size_t texture_id = 0;
                texture_id < scene.textures.size();
                ++texture_id) {

                const auto& source_texture =
                    scene.textures[texture_id];

                const UINT mip_count =
                    get_mip_count(source_texture);

                const auto description =
                    make_texture_description(source_texture);

                auto& texture =
                    output.textures[texture_id];

                texture.init(
                    device,
                    description,
                    dx::TextureType::texture2d,
                    D3D12_RESOURCE_STATE_COMMON);

                subresources.resize(mip_count);
                footprints.resize(mip_count);
                row_counts.resize(mip_count);
                row_sizes.resize(mip_count);

                UINT64 required_upload_size = 0;

                device->GetCopyableFootprints(
                    &description,
                    0,
                    mip_count,
                    0,
                    footprints.data(),
                    row_counts.data(),
                    row_sizes.data(),
                    &required_upload_size);

                for (std::uint32_t mip_id = 0;
                    mip_id < mip_count;
                    ++mip_id) {

                    const std::size_t source_mip_id =
                        static_cast<std::size_t>(
                            source_texture.mip_offset) +
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
                        scene.texture_data.data() + byte_offset,
                        source_mip.row_pitch,
                        source_mip.slice_pitch,
                        footprints[mip_id],
                        row_counts[mip_id],
                        row_sizes[mip_id],
                    };
                }

                uploader.upload_texture(
                    texture,
                    dx::TextureUploadDesc{
                        subresources,
                        required_upload_size,
                    },
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }
        }


        void create_texture_descriptors(
            DataPersistent& output,
            ID3D12Device* device,
            const scene::StaticScene& scene) {

            const UINT descriptor_count =
                std::max(
                    1u,
                    static_cast<UINT>(scene.textures.size()));

            output.texture_descriptors =
                output.texture_descriptors;

            const auto srgb =
                build_srgb_table(scene);

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
                    output.texture_descriptors.get_cpu());

                return;
            }

            for (std::size_t texture_id = 0;
                texture_id < scene.textures.size();
                ++texture_id) {

                const auto& source =
                    scene.textures[texture_id];

                const auto source_format =
                    static_cast<DXGI_FORMAT>(
                        source.dxgi_format);

                const DXGI_FORMAT view_format =
                    srgb[texture_id]
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


        [[nodiscard]]
        D3D12_SAMPLER_DESC make_default_sampler() noexcept {

            D3D12_SAMPLER_DESC description{};

            description.Filter =
                D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            description.AddressU =
                D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            description.AddressV =
                D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            description.AddressW =
                D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            description.MaxAnisotropy = 1;
            description.ComparisonFunc =
                D3D12_COMPARISON_FUNC_NEVER;
            description.MinLOD = 0.0f;
            description.MaxLOD = FLT_MAX;

            return description;
        }


        void create_samplers(
            DataPersistent& output,
            ID3D12Device* device,
            dx::DescriptorHeap& heap_sampler,
            const scene::StaticScene& scene) {

            if (scene.samplers.size() > MATERIAL_SAMPLER_COUNT) {
                log::Logger::g_logger << log::abrt(
                    "Scene uses more than two samplers.");
            }

            output.samplers =
                heap_sampler.alloc(MATERIAL_SAMPLER_COUNT);

            for (UINT sampler_id = 0;
                sampler_id < MATERIAL_SAMPLER_COUNT;
                ++sampler_id) {

                auto description =
                    make_default_sampler();

                if (sampler_id < scene.samplers.size()) {

                    const auto& source =
                        scene.samplers[sampler_id];

                    description.Filter =
                        static_cast<D3D12_FILTER>(
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

                    description.MaxAnisotropy =
                        std::clamp(
                            source.max_anisotropy,
                            1u,
                            16u);
                }

                dx::SamplerUtils::create(
                    device,
                    output.samplers.get_cpu(sampler_id),
                    description);
            }
        }


        void create_materials(
            DataPersistent& output,
            dx::ResourceUploader& uploader,
            ID3D12Device* device,
            const scene::StaticScene& scene) {

            std::vector<DataPersistent::Material> materials;
            materials.resize(scene.materials.size());

            for (std::size_t material_id = 0;
                material_id < scene.materials.size();
                ++material_id) {

                const auto& source =
                    scene.materials[material_id];

                auto& destination =
                    materials[material_id];

                destination.base_color = {
                    source.base_color.x,
                    source.base_color.y,
                    source.base_color.z,
                };

                destination.roughness =
                    source.roughness;

                destination.texture_basecolor =
                    get_texture_id(
                        scene,
                        source.texture_binding_base_color);

                destination.texture_normal =
                    get_texture_id(
                        scene,
                        source.texture_binding_normal);

                destination.texture_roughness =
                    get_texture_id(
                        scene,
                        source.texture_binding_roughness);

                destination.texture_opacity =
                    get_texture_id(
                        scene,
                        source.texture_binding_opacity);
            }

            upload_buffer(
                output.material,
                uploader,
                device,
                std::span<const DataPersistent::Material>{ materials },
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

    } // namespace

    void BuilderMaterial::build(
        data::DataPersistent& output,
        dx::ResourceUploader& uploader,
        ID3D12Device* device,
        dx::DescriptorHeap& heap_srv_cbv_uav,
        dx::DescriptorHeap& heap_sampler,
        const scene::StaticScene& scene) {

        const UINT descriptor_count =
            std::max(
                1u,
                static_cast<UINT>(scene.textures.size()));

        output.texture_descriptors =
            heap_srv_cbv_uav.alloc(descriptor_count);

        create_textures(
            output,
            uploader,
            device,
            scene);

        create_texture_descriptors(
            output,
            device,
            scene);

        create_samplers(
            output,
            device,
            heap_sampler,
            scene);

        create_materials(
            output,
            uploader,
            device,
            scene);
    }

} // namespace fjr::render