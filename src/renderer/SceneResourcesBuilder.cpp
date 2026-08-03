#include "FastJungle/renderer/SceneResourcesBuilder.hpp"

#include "FastJungle/dx12/DescriptorAllocator.hpp"
#include "FastJungle/dx12/WindowsUtils.hpp"

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace fjr::render {

    namespace {

        DXGI_FORMAT resource_format(DXGI_FORMAT format) noexcept {
            switch (format) {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
                return DXGI_FORMAT_R8G8B8A8_TYPELESS;
            case DXGI_FORMAT_BC1_UNORM:
            case DXGI_FORMAT_BC1_UNORM_SRGB:
                return DXGI_FORMAT_BC1_TYPELESS;
            case DXGI_FORMAT_BC2_UNORM:
            case DXGI_FORMAT_BC2_UNORM_SRGB:
                return DXGI_FORMAT_BC2_TYPELESS;
            case DXGI_FORMAT_BC3_UNORM:
            case DXGI_FORMAT_BC3_UNORM_SRGB:
                return DXGI_FORMAT_BC3_TYPELESS;
            case DXGI_FORMAT_BC7_UNORM:
            case DXGI_FORMAT_BC7_UNORM_SRGB:
                return DXGI_FORMAT_BC7_TYPELESS;
            default:
                return format;
            }
        }

        DXGI_FORMAT linear_view_format(DXGI_FORMAT format) noexcept {
            switch (format) {
            case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
                return DXGI_FORMAT_R8G8B8A8_UNORM;
            case DXGI_FORMAT_BC1_UNORM_SRGB:
                return DXGI_FORMAT_BC1_UNORM;
            case DXGI_FORMAT_BC2_UNORM_SRGB:
                return DXGI_FORMAT_BC2_UNORM;
            case DXGI_FORMAT_BC3_UNORM_SRGB:
                return DXGI_FORMAT_BC3_UNORM;
            case DXGI_FORMAT_BC7_UNORM_SRGB:
                return DXGI_FORMAT_BC7_UNORM;
            default:
                return format;
            }
        }

        DXGI_FORMAT srgb_view_format(DXGI_FORMAT format) noexcept {
            switch (format) {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
                return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            case DXGI_FORMAT_BC1_UNORM:
                return DXGI_FORMAT_BC1_UNORM_SRGB;
            case DXGI_FORMAT_BC2_UNORM:
                return DXGI_FORMAT_BC2_UNORM_SRGB;
            case DXGI_FORMAT_BC3_UNORM:
                return DXGI_FORMAT_BC3_UNORM_SRGB;
            case DXGI_FORMAT_BC7_UNORM:
                return DXGI_FORMAT_BC7_UNORM_SRGB;
            default:
                return format;
            }
        }

        template<typename T>
        void upload_static_buffer(
            dx::Buffer& destination,
            std::vector<dx::Buffer>& upload_buffers,
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            std::span<const T> source,
            D3D12_RESOURCE_STATES final_state) {

            if (source.empty()) {
                return;
            }

            const UINT64 byte_size =
                static_cast<UINT64>(source.size_bytes());

            destination.init(
                device,
                byte_size,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_FLAG_NONE,
                D3D12_RESOURCE_STATE_COPY_DEST);

            upload_buffers.emplace_back();
            auto& upload = upload_buffers.back();
            upload.init(
                device,
                byte_size,
                D3D12_HEAP_TYPE_UPLOAD,
                D3D12_RESOURCE_FLAG_NONE,
                D3D12_RESOURCE_STATE_GENERIC_READ);

            void* mapped = nullptr;
            const D3D12_RANGE read_range{0, 0};
            dx::abort_failed(upload->Map(0, &read_range, &mapped));
            std::memcpy(mapped, source.data(), source.size_bytes());
            const D3D12_RANGE written_range{
                0,
                static_cast<SIZE_T>(byte_size),
            };
            upload->Unmap(0, &written_range);

            command_list->CopyBufferRegion(
                destination.get(),
                0,
                upload.get(),
                0,
                byte_size);
            destination.transition(command_list, final_state);
        }

        void create_camera_buffer(
            SceneResources& resources,
            ID3D12Device* device,
            std::uint32_t frame_count) {

            const UINT64 byte_size =
                static_cast<UINT64>(
                    sizeof(SceneResources::CameraConstants)) *
                frame_count;

            resources.buf_cbuffer_camera.init(
                device,
                byte_size,
                D3D12_HEAP_TYPE_UPLOAD,
                D3D12_RESOURCE_FLAG_NONE,
                D3D12_RESOURCE_STATE_GENERIC_READ);

            void* mapped = nullptr;
            const D3D12_RANGE read_range{0, 0};
            dx::abort_failed(resources.buf_cbuffer_camera->Map(
                0,
                &read_range,
                &mapped));

            const SceneResources::CameraConstants initial{};
            for (std::uint32_t frame = 0;
                frame < frame_count;
                ++frame) {
                std::memcpy(
                    static_cast<std::byte*>(mapped) +
                        static_cast<std::size_t>(frame) * sizeof(initial),
                    &initial,
                    sizeof(initial));
            }

            const D3D12_RANGE written_range{
                0,
                static_cast<SIZE_T>(byte_size),
            };
            resources.buf_cbuffer_camera->Unmap(
                0,
                &written_range);
        }

        void append_material_data(
            SceneResources& resources,
            const scene::StaticScene& scene) {

            resources.texture_binding_data.reserve(
                scene.texture_bindings.size() + 1);
            for (const auto& source : scene.texture_bindings) {
                SceneResources::TextureBinding binding;
                binding.texture_id = source.texture;
                binding.sampler_id = source.sampler;
                binding.channel = static_cast<std::uint32_t>(
                    source.channel);
                binding.flags = static_cast<std::uint32_t>(
                    source.flags);
                resources.texture_binding_data.push_back(binding);
            }
            resources.texture_binding_data.emplace_back();

            resources.material_data.reserve(scene.materials.size() + 1);
            for (const auto& source : scene.materials) {
                SceneResources::Material material;
                material.base_color = source.base_color;
                material.emissive_roughness = {
                    source.emissive.x,
                    source.emissive.y,
                    source.emissive.z,
                    source.roughness,
                };
                material.surface = {
                    source.metallic,
                    source.opacity,
                    source.opacity_threshold,
                    0.0f,
                };
                material.texture_bindings_0 = {
                    source.texture_binding_base_color,
                    source.texture_binding_normal,
                    source.texture_binding_roughness,
                    source.texture_binding_opacity,
                };
                material.texture_bindings_1.x =
                    source.texture_binding_emissive;
                resources.material_data.push_back(material);
            }

            resources.default_material_id =
                static_cast<std::uint32_t>(
                    resources.material_data.size());
            resources.material_data.emplace_back();
        }

        bool append_part_draws(
            SceneResources& resources,
            const scene::StaticScene& scene,
            const scene::StaticScene::PrototypePart& part,
            SceneResources::InstanceKind instance_kind,
            std::uint32_t instance_offset,
            std::uint32_t instance_count,
            std::uint32_t transform_constant_index) {

            const auto& mesh = scene.meshes[part.mesh];
            bool appended = false;

            for (std::uint32_t local_submesh = 0;
                local_submesh < mesh.submesh_count;
                ++local_submesh) {
                const auto& submesh = scene.submeshes[
                    static_cast<std::size_t>(mesh.submesh_offset) +
                    local_submesh];

                if (submesh.index_count == 0 || instance_count == 0) {
                    continue;
                }

                SceneResources::DrawItem draw;
                draw.instance_kind = instance_kind;
                draw.index_count = submesh.index_count;
                draw.first_index = submesh.index_offset;
                draw.base_vertex = static_cast<std::int32_t>(
                    submesh.vertex_offset);
                draw.instance_count = instance_count;
                draw.constants.instance_offset = instance_offset;
                draw.constants.material_id =
                    submesh.material == scene::StaticScene::INVALID_INDEX
                    ? resources.default_material_id
                    : submesh.material;
                draw.constants.instance_kind =
                    static_cast<std::uint32_t>(instance_kind);
                draw.transform_constant_index =
                    transform_constant_index;
                draw.flags = submesh.flags;

                resources.draw_items.push_back(draw);
                appended = true;
            }

            return appended;
        }

        void append_point_draws(
            SceneResources& resources,
            const scene::StaticScene& scene) {

            for (const auto& batch : scene.point_batches) {
                if (batch.instance_count == 0) {
                    continue;
                }

                const auto& prototype =
                    scene.prototypes[batch.prototype];

                for (std::uint32_t local_part = 0;
                    local_part < prototype.part_count;
                    ++local_part) {
                    const auto& part = scene.prototype_parts[
                        static_cast<std::size_t>(prototype.part_offset) +
                        local_part];

                    const auto constant_index =
                        static_cast<std::uint32_t>(
                            resources.point_draw_constants.size());

                    const bool appended = append_part_draws(
                        resources,
                        scene,
                        part,
                        SceneResources::InstanceKind::POINT,
                        batch.instance_offset,
                        batch.instance_count,
                        constant_index);

                    if (appended) {
                        SceneResources::PointDrawConstants constants;
                        constants.part_local_transform =
                            part.local_transform;
                        constants.batch_local_to_world =
                            batch.local_to_world;
                        resources.point_draw_constants.push_back(constants);
                    }
                }
            }
        }

        void append_matrix_draws(
            SceneResources& resources,
            const scene::StaticScene& scene) {

            std::vector<std::uint32_t> part_constant_indices(
                scene.prototype_parts.size(),
                scene::StaticScene::INVALID_INDEX);

            for (const auto& batch : scene.matrix_batches) {
                if (batch.instance_count == 0) {
                    continue;
                }

                const auto& prototype =
                    scene.prototypes[batch.prototype];

                for (std::uint32_t local_part = 0;
                    local_part < prototype.part_count;
                    ++local_part) {
                    const auto part_index =
                        static_cast<std::size_t>(prototype.part_offset) +
                        local_part;
                    const auto& part = scene.prototype_parts[part_index];

                    auto constant_index =
                        part_constant_indices[part_index];
                    const bool needs_constant =
                        constant_index == scene::StaticScene::INVALID_INDEX;
                    if (needs_constant) {
                        constant_index = static_cast<std::uint32_t>(
                            resources.matrix_draw_constants.size());
                    }

                    const bool appended = append_part_draws(
                        resources,
                        scene,
                        part,
                        SceneResources::InstanceKind::MATRIX,
                        batch.instance_offset,
                        batch.instance_count,
                        constant_index);

                    if (appended && needs_constant) {
                        SceneResources::MatrixDrawConstants constants;
                        constants.part_local_transform =
                            part.local_transform;
                        resources.matrix_draw_constants.push_back(constants);
                        part_constant_indices[part_index] = constant_index;
                    }
                }
            }
        }

        void create_texture_resources(
            SceneResources& resources,
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            const scene::StaticScene& scene) {

            const UINT texture_descriptor_count = std::max(
                1u,
                static_cast<UINT>(scene.textures.size()) * 2u);

            resources.heap_srv.init(
                device,
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                texture_descriptor_count,
                true);

            dx::DescriptorAllocator descriptor_allocator;
            descriptor_allocator.init(texture_descriptor_count);
            auto descriptor_region =
                descriptor_allocator.allocate_region(
                    texture_descriptor_count);

            if (scene.textures.empty()) {
                const auto allocation = descriptor_region.allocate();
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
                    resources.heap_srv.get_cpu_handle(allocation, 0));
                return;
            }

            resources.textures.resize(scene.textures.size());

            for (std::size_t texture_index = 0;
                texture_index < scene.textures.size();
                ++texture_index) {
                const auto& source_texture =
                    scene.textures[texture_index];
                const auto source_format = static_cast<DXGI_FORMAT>(
                    source_texture.dxgi_format);

                D3D12_RESOURCE_DESC description{};
                description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                description.Width = source_texture.width;
                description.Height = source_texture.height;
                description.DepthOrArraySize = 1;
                description.MipLevels = static_cast<UINT16>(
                    source_texture.mip_count);
                description.Format = resource_format(source_format);
                description.SampleDesc.Count = 1;
                description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
                description.Flags = D3D12_RESOURCE_FLAG_NONE;

                auto& texture = resources.textures[texture_index];
                texture.init(
                    device,
                    description,
                    dx::TextureType::texture2d,
                    D3D12_RESOURCE_STATE_COPY_DEST);

                std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(
                    source_texture.mip_count);
                std::vector<UINT> row_counts(source_texture.mip_count);
                std::vector<UINT64> row_sizes(source_texture.mip_count);
                UINT64 upload_size = 0;
                device->GetCopyableFootprints(
                    &description,
                    0,
                    source_texture.mip_count,
                    0,
                    footprints.data(),
                    row_counts.data(),
                    row_sizes.data(),
                    &upload_size);

                resources.upload_buffers.emplace_back();
                auto& upload = resources.upload_buffers.back();
                upload.init(
                    device,
                    upload_size,
                    D3D12_HEAP_TYPE_UPLOAD,
                    D3D12_RESOURCE_FLAG_NONE,
                    D3D12_RESOURCE_STATE_GENERIC_READ);

                void* mapped = nullptr;
                const D3D12_RANGE read_range{0, 0};
                dx::abort_failed(upload->Map(
                    0,
                    &read_range,
                    &mapped));

                for (std::uint32_t mip_index = 0;
                    mip_index < source_texture.mip_count;
                    ++mip_index) {
                    const auto& source_mip = scene.texture_mips[
                        static_cast<std::size_t>(source_texture.mip_offset) +
                        mip_index];
                    const auto* source = scene.texture_data.data() +
                        source_texture.data_byte_offset +
                        source_mip.data_byte_offset_local;
                    auto* destination =
                        static_cast<std::byte*>(mapped) +
                        footprints[mip_index].Offset;

                    for (UINT row = 0;
                        row < row_counts[mip_index];
                        ++row) {
                        std::memcpy(
                            destination +
                                static_cast<std::size_t>(row) *
                                footprints[mip_index].Footprint.RowPitch,
                            source +
                                static_cast<std::size_t>(row) *
                                source_mip.row_pitch,
                            source_mip.row_pitch);
                    }
                }

                const D3D12_RANGE written_range{
                    0,
                    static_cast<SIZE_T>(upload_size),
                };
                upload->Unmap(0, &written_range);

                for (std::uint32_t mip_index = 0;
                    mip_index < source_texture.mip_count;
                    ++mip_index) {
                    D3D12_TEXTURE_COPY_LOCATION destination{};
                    destination.pResource = texture.get();
                    destination.Type =
                        D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                    destination.SubresourceIndex = mip_index;

                    D3D12_TEXTURE_COPY_LOCATION source{};
                    source.pResource = upload.get();
                    source.Type =
                        D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                    source.PlacedFootprint = footprints[mip_index];

                    command_list->CopyTextureRegion(
                        &destination,
                        0,
                        0,
                        0,
                        &source,
                        nullptr);
                }

                texture.transition(
                    command_list,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

                const auto allocation = descriptor_region.allocate(2);
                (void)texture.create_srv(
                    device,
                    resources.heap_srv,
                    allocation,
                    {},
                    linear_view_format(source_format),
                    0);
                (void)texture.create_srv(
                    device,
                    resources.heap_srv,
                    allocation,
                    {},
                    srgb_view_format(source_format),
                    1);
            }
        }

        void create_sampler_resources(
            SceneResources& resources,
            ID3D12Device* device,
            const scene::StaticScene& scene) {

            const UINT sampler_count = std::max(
                1u,
                static_cast<UINT>(scene.samplers.size()));
            resources.heap_samplers.init(
                device,
                D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                sampler_count,
                true);

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

                device->CreateSampler(
                    &description,
                    resources.heap_samplers.get_cpu_handle(sampler_index));
            }
        }

    } // namespace

    std::unique_ptr<SceneResources> SceneResourcesBuilder::build(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* command_list,
        std::uint32_t frame_count,
        const scene::StaticScene& scene) {

        auto resources = std::make_unique<SceneResources>();

        resources->environment_light = scene.environment_light;
        resources->scene_info = scene.info;

        append_material_data(*resources, scene);
        append_point_draws(*resources, scene);
        append_matrix_draws(*resources, scene);

        const auto alpha_blended = static_cast<std::uint32_t>(
            scene::StaticScene::EnumSubmeshFlag::ALPHA_BLENDED);
        std::stable_partition(
            resources->draw_items.begin(),
            resources->draw_items.end(),
            [alpha_blended](const SceneResources::DrawItem& draw) {
                return (
                    static_cast<std::uint32_t>(draw.flags) &
                    alpha_blended) == 0;
            });

        resources->draw_data.reserve(resources->draw_items.size());
        for (const auto& draw : resources->draw_items) {
            SceneResources::DrawData data;
            data.first_index = draw.first_index;
            data.base_vertex = draw.base_vertex;
            data.instance_offset = draw.constants.instance_offset;
            data.material_id = draw.constants.material_id;
            data.instance_kind = draw.constants.instance_kind;
            data.transform_constant_index =
                draw.transform_constant_index;
            resources->draw_data.push_back(data);
        }
        if (resources->draw_data.empty()) {
            resources->draw_data.emplace_back();
        }

        upload_static_buffer(
            resources->buf_vertices,
            resources->upload_buffers,
            device,
            command_list,
            std::span<const scene::StaticScene::Vertex>{scene.vertices},
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        upload_static_buffer(
            resources->buf_indices,
            resources->upload_buffers,
            device,
            command_list,
            std::span<const std::uint32_t>{scene.indices},
            D3D12_RESOURCE_STATE_INDEX_BUFFER);
        upload_static_buffer(
            resources->buf_instances_point,
            resources->upload_buffers,
            device,
            command_list,
            std::span<const scene::StaticScene::PointInstance>{
                scene.point_instances},
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        upload_static_buffer(
            resources->buf_instances_matrix,
            resources->upload_buffers,
            device,
            command_list,
            std::span<const scene::StaticScene::MatrixInstance>{
                scene.matrix_instances},
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        upload_static_buffer(
            resources->buf_materials,
            resources->upload_buffers,
            device,
            command_list,
            std::span<const SceneResources::Material>{
                resources->material_data},
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        upload_static_buffer(
            resources->buf_texture_bindings,
            resources->upload_buffers,
            device,
            command_list,
            std::span<const SceneResources::TextureBinding>{
                resources->texture_binding_data},
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        upload_static_buffer(
            resources->buf_draw_data,
            resources->upload_buffers,
            device,
            command_list,
            std::span<const SceneResources::DrawData>{
                resources->draw_data},
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        upload_static_buffer(
            resources->buf_cbuffer_point,
            resources->upload_buffers,
            device,
            command_list,
            std::span<const SceneResources::PointDrawConstants>{
                resources->point_draw_constants},
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        upload_static_buffer(
            resources->buf_cbuffer_matrix,
            resources->upload_buffers,
            device,
            command_list,
            std::span<const SceneResources::MatrixDrawConstants>{
                resources->matrix_draw_constants},
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

        create_camera_buffer(*resources, device, frame_count);
        create_texture_resources(
            *resources,
            device,
            command_list,
            scene);
        create_sampler_resources(*resources, device, scene);

        if (resources->buf_vertices) {
            resources->view_vertices.BufferLocation =
                resources->buf_vertices->GetGPUVirtualAddress();
            resources->view_vertices.SizeInBytes = static_cast<UINT>(
                scene.vertices.size() *
                sizeof(scene::StaticScene::Vertex));
            resources->view_vertices.StrideInBytes =
                sizeof(scene::StaticScene::Vertex);
        }
        if (resources->buf_indices) {
            resources->view_indices.BufferLocation =
                resources->buf_indices->GetGPUVirtualAddress();
            resources->view_indices.SizeInBytes = static_cast<UINT>(
                scene.indices.size() * sizeof(std::uint32_t));
            resources->view_indices.Format = DXGI_FORMAT_R32_UINT;
        }

        return resources;
    }

} // namespace fjr::render
