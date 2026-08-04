#include "FastJungle/renderer/SceneResourcesBuilder.hpp"

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include "FastJungle/dx12/WindowsUtils.hpp"
#include "FastJungle/dx12/FormatUtils.hpp"
#include "FastJungle/dx12/HeapManager.hpp"

namespace fjr::render {

    namespace {


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
                    source.ior,
                };
                material.optical = {
                    source.specular,
                    source.clearcoat,
                    source.clearcoat_roughness,
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
                material.texture_bindings_1.y =
                    source.texture_binding_metallic;
                resources.material_data.push_back(material);
            }

            resources.default_material_id =
                static_cast<std::uint32_t>(
                    resources.material_data.size());
            resources.material_data.emplace_back();
        }

        bool append_mesh_draws(
            SceneResources& resources,
            const scene::StaticScene& scene,
            std::uint32_t mesh_index,
            SceneResources::Component component,
            SceneResources::InstanceKind instance_kind,
            std::uint32_t instance_offset,
            std::uint32_t instance_count,
            std::uint32_t transform_constant_index,
            std::uint32_t bounds_index) {

            const auto& mesh = scene.meshes[mesh_index];
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
                draw.component = component;
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
                draw.bounds_index = bounds_index;
                draw.flags = submesh.flags;

                resources.draw_items.push_back(draw);
                appended = true;
            }

            return appended;
        }

        void append_point_range(
            SceneResources& resources,
            const scene::StaticScene& scene,
            scene::StaticScene::IndexRange range,
            SceneResources::Component component) {

            for (std::uint32_t local_batch = 0;
                 local_batch < range.count;
                 ++local_batch) {
                const auto batch_index = range.offset + local_batch;
                const auto& batch = scene.point_batches[batch_index];
                if (batch.instance_count == 0) {
                    continue;
                }

                const auto& definition =
                    scene.instanced_mesh_definitions[batch.definition];
                const auto constant_index = static_cast<std::uint32_t>(
                    resources.point_draw_constants.size());

                const bool appended = append_mesh_draws(
                    resources,
                    scene,
                    definition.mesh,
                    component,
                    SceneResources::InstanceKind::POINT,
                    batch.instance_offset,
                    batch.instance_count,
                    constant_index,
                    batch_index);

                if (appended) {
                    SceneResources::PointDrawConstants constants;
                    constants.part_local_transform =
                        definition.local_transform;
                    constants.batch_local_to_world =
                        batch.local_to_world;
                    resources.point_draw_constants.push_back(constants);
                }
            }
        }

        void append_point_draws(
            SceneResources& resources,
            const scene::StaticScene& scene) {

            const auto& components = scene.components;
            append_point_range(resources, scene,
                components.anthurium.point_batches,
                SceneResources::Component::ANTHURIUM);
            append_point_range(resources, scene,
                components.nettle.point_batches,
                SceneResources::Component::NETTLE);
            append_point_range(resources, scene,
                components.shrub_sorrel.point_batches,
                SceneResources::Component::SHRUB_SORREL);
            append_point_range(resources, scene,
                components.shrub.point_batches,
                SceneResources::Component::SHRUB);
            append_point_range(resources, scene,
                components.grass_b.point_batches,
                SceneResources::Component::GRASS_B);
            append_point_range(resources, scene,
                components.grass_a.point_batches,
                SceneResources::Component::GRASS_A);
            append_point_range(resources, scene,
                components.pyramid_grass_b.point_batches,
                SceneResources::Component::PYRAMID_GRASS_B);
            append_point_range(resources, scene,
                components.pyramid_moss.point_batches,
                SceneResources::Component::PYRAMID_MOSS);
            append_point_range(resources, scene,
                components.queen_forest.point_batches,
                SceneResources::Component::QUEEN_FOREST);
            append_point_range(resources, scene,
                components.river_forest.point_batches,
                SceneResources::Component::RIVER_FOREST);
            append_point_range(resources, scene,
                components.river_sapling.point_batches,
                SceneResources::Component::RIVER_SAPLING);
            append_point_range(resources, scene,
                components.river_seedling.point_batches,
                SceneResources::Component::RIVER_SEEDLING);
        }

        void append_static_instance(
            SceneResources& resources,
            const scene::StaticScene& scene,
            std::uint32_t instance_index,
            SceneResources::Component component) {

            const auto& instance =
                scene.static_mesh_instances[instance_index];
            append_mesh_draws(
                resources,
                scene,
                instance.mesh,
                component,
                SceneResources::InstanceKind::MATRIX,
                instance_index,
                1,
                0,
                instance_index);
        }

        void append_static_range(
            SceneResources& resources,
            const scene::StaticScene& scene,
            scene::StaticScene::IndexRange range,
            SceneResources::Component component) {

            for (std::uint32_t local_instance = 0;
                 local_instance < range.count;
                 ++local_instance) {
                append_static_instance(
                    resources,
                    scene,
                    range.offset + local_instance,
                    component);
            }
        }

        void append_static_draws(
            SceneResources& resources,
            const scene::StaticScene& scene) {

            resources.matrix_instance_data.reserve(
                scene.static_mesh_instances.size());
            for (const auto& source : scene.static_mesh_instances) {
                SceneResources::MatrixInstance destination;
                destination.transform = source.world_transform;
                resources.matrix_instance_data.push_back(destination);
            }

            if (!scene.static_mesh_instances.empty()) {
                // All static instance matrices are complete world transforms,
                // so every static draw shares the identity draw transform.
                resources.matrix_draw_constants.emplace_back();
            }

            const auto& components = scene.components;
            append_static_instance(resources, scene,
                components.pyramid.instance,
                SceneResources::Component::PYRAMID);
            append_static_instance(resources, scene,
                components.river.instance,
                SceneResources::Component::RIVER);
            append_static_instance(resources, scene,
                components.creek.instance,
                SceneResources::Component::CREEK);
            append_static_instance(resources, scene,
                components.banyan.instance,
                SceneResources::Component::BANYAN);
            append_static_range(resources, scene,
                components.terrain.extended,
                SceneResources::Component::TERRAIN);
            append_static_range(resources, scene,
                components.terrain.cinematic,
                SceneResources::Component::TERRAIN);
        }

        void create_texture_resources(
            SceneResources& resources,
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            const scene::StaticScene& scene) {

            const UINT texture_descriptor_count = std::max(
                1u,
                static_cast<UINT>(scene.textures.size()) * 2u);

            resources.texture_descriptors =
                dx::HeapManager::g_heap_manager.heap_srv_cbv_uav
                    .alloc(texture_descriptor_count);

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

            for (std::size_t texture_index = 0;
                texture_index < scene.textures.size();
                ++texture_index) {
                auto& source_texture =
                    scene.textures[texture_index];
                const auto source_format = static_cast<DXGI_FORMAT>(
                    source_texture.dxgi_format);

                // i edit this mamually
                int mip_count = 1; // source_texture.mip_count = 1;

                D3D12_RESOURCE_DESC description{};
                description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                description.Width = source_texture.width;
                description.Height = source_texture.height;
                description.DepthOrArraySize = 1;
                description.MipLevels = mip_count;
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

                std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(
                    mip_count);
                std::vector<UINT> row_counts(mip_count);
                std::vector<UINT64> row_sizes(mip_count);
                UINT64 upload_size = 0;
                device->GetCopyableFootprints(
                    &description,
                    0,
                    mip_count,
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
                    mip_index < mip_count;
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
                    mip_index < mip_count;
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

                (void)texture.create_srv(
                    device,
                    resources.texture_descriptors.get_cpu(
                        static_cast<UINT>(texture_index) * 2u),
                    dx::TextureViewRange{ 0, 1, 0, 1 },
                    dx::FormatUtils::to_linear(source_format));
                (void)texture.create_srv(
                    device,
                    resources.texture_descriptors.get_cpu(
                        static_cast<UINT>(texture_index) * 2u + 1u),
                    dx::TextureViewRange{0, 1, 0, 1},
                    dx::FormatUtils::to_srgb(source_format));
            }
        }

        void create_sampler_resources(
            SceneResources& resources,
            ID3D12Device* device,
            const scene::StaticScene& scene) {

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

                device->CreateSampler(
                    &description,
                    resources.sampler_descriptors.get_cpu(sampler_index));
            }
        }

    } // namespace

    std::unique_ptr<SceneResources> SceneResourcesBuilder::build(
        BuildContexts& context,
        const scene::StaticScene& scene) {

        auto resources = std::make_unique<SceneResources>();

        resources->environment_light = scene.environment_light;

        append_material_data(*resources, scene);
        append_point_draws(*resources, scene);
        append_static_draws(*resources, scene);

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
            context.device,
            context.context,
            std::span<const scene::StaticScene::Vertex>{scene.vertices},
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        upload_static_buffer(
            resources->buf_indices,
            resources->upload_buffers,
            context.device,
            context.context,
            std::span<const std::uint32_t>{scene.indices},
            D3D12_RESOURCE_STATE_INDEX_BUFFER);
        upload_static_buffer(
            resources->buf_instances_point,
            resources->upload_buffers,
            context.device,
            context.context,
            std::span<const scene::StaticScene::PointInstance>{
                scene.point_instances},
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        upload_static_buffer(
            resources->buf_instances_matrix,
            resources->upload_buffers,
            context.device,
            context.context,
            std::span<const SceneResources::MatrixInstance>{
                resources->matrix_instance_data},
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        upload_static_buffer(
            resources->buf_materials,
            resources->upload_buffers,
            context.device,
            context.context,
            std::span<const SceneResources::Material>{
                resources->material_data},
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        upload_static_buffer(
            resources->buf_texture_bindings,
            resources->upload_buffers,
            context.device,
            context.context,
            std::span<const SceneResources::TextureBinding>{
                resources->texture_binding_data},
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        upload_static_buffer(
            resources->buf_draw_data,
            resources->upload_buffers,
            context.device,
            context.context,
            std::span<const SceneResources::DrawData>{
                resources->draw_data},
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        upload_static_buffer(
            resources->buf_cbuffer_point,
            resources->upload_buffers,
            context.device,
            context.context,
            std::span<const SceneResources::PointDrawConstants>{
                resources->point_draw_constants},
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        upload_static_buffer(
            resources->buf_cbuffer_matrix,
            resources->upload_buffers,
            context.device,
            context.context,
            std::span<const SceneResources::MatrixDrawConstants>{
                resources->matrix_draw_constants},
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

        resources->view_cbuf_transform_matrix = dx::CBufferArrayView(
            resources->buf_cbuffer_matrix->GetGPUVirtualAddress(),
            sizeof(SceneResources::MatrixDrawConstants));
        resources->view_cbuf_transform_point = dx::CBufferArrayView(
            resources->buf_cbuffer_point->GetGPUVirtualAddress(),
            sizeof(SceneResources::PointDrawConstants));

        create_texture_resources(
            *resources,
            context.device,
            context.context,
            scene);
        create_sampler_resources(*resources, context.device, scene);

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
