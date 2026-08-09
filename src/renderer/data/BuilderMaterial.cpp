#include "FastJungle/renderer/data/BuilderMaterial.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
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
        constexpr std::uint32_t MATERIAL_FLAG_IMPOSTOR = 1u << 0u;


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


        [[nodiscard]] std::uint32_t create_water_sky_texture(
            DataPersistent& output,
            dx::ResourceUploader& uploader,
            ID3D12Device* device) {

            constexpr std::array<std::byte, 4> pixels{
                std::byte{0x78},
                std::byte{0xc9},
                std::byte{0xf4},
                std::byte{0xff},
            };

            D3D12_RESOURCE_DESC description{};
            description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            description.Width = 1;
            description.Height = 1;
            description.DepthOrArraySize = 1;
            description.MipLevels = 1;
            description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            description.SampleDesc.Count = 1;
            description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
            UINT row_count = 0;
            UINT64 row_size = 0;
            UINT64 required_upload_size = 0;
            device->GetCopyableFootprints(
                &description,
                0,
                1,
                0,
                &footprint,
                &row_count,
                &row_size,
                &required_upload_size);

            const auto texture_id = static_cast<std::uint32_t>(
                output.textures.size());
            auto& texture = output.textures.emplace_back();
            texture.init(
                device,
                description,
                dx::TextureType::texture2d,
                D3D12_RESOURCE_STATE_COMMON);
            const std::array subresources{
                dx::TextureSubresourceData{
                    pixels.data(),
                    pixels.size(),
                    pixels.size(),
                    footprint,
                    row_count,
                    row_size,
                },
            };
            uploader.upload_texture(
                texture,
                dx::TextureUploadDesc{
                    subresources,
                    required_upload_size,
                },
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            return texture_id;
        }


        void create_texture_descriptors(
            DataPersistent& output,
            ID3D12Device* device,
            const scene::StaticScene& scene) {

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


        void create_water_sky_texture_descriptor(
            DataPersistent& output,
            ID3D12Device* device,
            std::uint32_t texture_id) {

            output.textures[texture_id].create_srv(
                device,
                output.texture_descriptors.get_cpu(texture_id),
                dx::TextureViewRange{0, 1, 0, 1},
                DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
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


        void select_pipeline_samplers(
            DataPersistent& output,
            const scene::StaticScene& scene) {

            output.wrap_sampler = Consts::IND_ERR;
            output.clamp_sampler = Consts::IND_ERR;
            for (std::uint32_t sampler_id = 0;
                sampler_id < scene.samplers.size();
                ++sampler_id) {

                const auto& sampler = scene.samplers[sampler_id];
                if (sampler.address_u ==
                    scene::StaticScene::EnumSamplerAddressMode::WRAP &&
                    sampler.address_v ==
                    scene::StaticScene::EnumSamplerAddressMode::WRAP) {

                    output.wrap_sampler = sampler_id;
                }
                if (sampler.address_u ==
                    scene::StaticScene::EnumSamplerAddressMode::CLAMP &&
                    sampler.address_v ==
                    scene::StaticScene::EnumSamplerAddressMode::CLAMP) {

                    output.clamp_sampler = sampler_id;
                }
            }

            if (output.wrap_sampler == Consts::IND_ERR ||
                output.clamp_sampler == Consts::IND_ERR) {

                log::Logger::g_logger << log::abrt(
                    "Scene must provide WRAP and CLAMP samplers.");
            }
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
            const scene::StaticScene& scene,
            std::span<const DataPersistent::Mesh> meshes,
            std::uint32_t water_sky_texture) {

            std::vector<DataPersistent::Material> materials;
            materials.resize(scene.materials.size());

            std::vector<bool> water_materials(scene.materials.size());
            const auto collect_water_materials =
                [&scene, &water_materials](std::uint32_t instance_id) {

                    const auto& instance = scene.static_mesh_instances[
                        instance_id];
                    const auto& mesh = scene.meshes[instance.mesh];
                    for (std::uint32_t lod_id = 0;
                        lod_id < mesh.lod_count;
                        ++lod_id) {

                        const auto& lod = scene.mesh_lods[
                            mesh.lod_offset + lod_id];
                        for (std::uint32_t submesh_id = 0;
                            submesh_id < lod.submesh_count;
                            ++submesh_id) {

                            water_materials[scene.submeshes[
                                lod.submesh_offset + submesh_id]
                                .material] = true;
                        }
                    }
                };
            collect_water_materials(scene.components.river.instance);
            collect_water_materials(scene.components.creek.instance);

            for (const auto& impostor : scene.impostors) {
                const auto center = meshes[impostor.mesh].bounds_center;
                for (std::uint32_t direction = 0;
                    direction < impostor.direction_count;
                    ++direction) {
                    const auto& card_mesh = scene.meshes[
                        impostor.card_mesh_offset + direction];
                    const auto& card_lod = scene.mesh_lods[card_mesh.lod_offset];
                    const auto& card = scene.submeshes[card_lod.submesh_offset];

                    float half_width = 0.0f;
                    float half_height = 0.0f;
                    for (std::uint32_t vertex = 0;
                        vertex < card.vertex_count;
                        ++vertex) {
                        const auto& position = scene.vertices[
                            card.vertex_offset + vertex].position;
                        const float x = position.x - center.x;
                        const float y = position.y - center.y;
                        const float z = position.z - center.z;
                        half_width = (std::max)(
                            half_width,
                            std::sqrt(x * x + z * z));
                        half_height = (std::max)(half_height, std::abs(y));
                    }
                    auto& material = materials[card.material];
                    material.flags |= MATERIAL_FLAG_IMPOSTOR;
                    material.impostor_center = center;
                    material.impostor_half_width = half_width;
                    material.impostor_half_height = half_height;
                }
            }

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

                if (water_materials[material_id]) {
                    destination.base_color = {1.0f, 1.0f, 1.0f};
                    destination.texture_basecolor = water_sky_texture;
                    destination.texture_opacity = Consts::IND_ERR;
                }
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
            const scene::StaticScene& scene,
            std::span<const DataPersistent::Mesh> meshes) {

        const UINT descriptor_count = static_cast<UINT>(
            scene.textures.size() + 1);

        output.texture_descriptors =
            heap_srv_cbv_uav.alloc(descriptor_count);

        create_textures(
            output,
            uploader,
            device,
            scene);

        const auto water_sky_texture = create_water_sky_texture(
            output,
            uploader,
            device);

        create_texture_descriptors(
            output,
            device,
            scene);

        create_water_sky_texture_descriptor(
            output,
            device,
            water_sky_texture);

        create_samplers(
            output,
            device,
            heap_sampler,
            scene);

        select_pipeline_samplers(output, scene);

        create_materials(
            output,
            uploader,
            device,
            scene,
            meshes,
            water_sky_texture);
    }

} // namespace fjr::render
