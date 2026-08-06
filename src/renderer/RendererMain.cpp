#include "FastJungle/renderer/RendererMain.hpp"

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <utility>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/dx12/HeapManager.hpp"
#include "FastJungle/dx12/WindowsUtils.hpp"
#include "FastJungle/renderer/builder/SceneBoundsBuilder.hpp"
#include "FastJungle/renderer/builder/SceneDynamicDataBuilder.hpp"
#include "FastJungle/renderer/builder/SceneFrameConstDataBuilder.hpp"
#include "FastJungle/renderer/builder/PointCullingDataBuilder.hpp"
#include "FastJungle/renderer/builder/SceneResourcesBuilder.hpp"
#include "FastJungle/renderer/builder/SceneResourcesTempBuilder.hpp"
#include "FastJungle/renderer/data/RenderConsts.hpp"

namespace fjr::render {

    namespace {

        [[nodiscard]]
        bool positive_finite(float value) noexcept {
            return std::isfinite(value) && value > 1.0e-6f;
        }

        void initialize_camera(
            Camera& camera,
            const scene::StaticScene::Camera& source,
            const math::AABB& bounds,
            std::uint32_t width,
            std::uint32_t height,
            bool frame_entire_scene) noexcept {

            const float aspect_ratio =
                static_cast<float>(std::max(width, 1u)) /
                static_cast<float>(std::max(height, 1u));

            if (frame_entire_scene) {
                camera.init(
                    {}, {},
                    DirectX::XM_PIDIV4, aspect_ratio,
                    0.01f, 100.0f,
                    1.0f, 0.04f);
                camera.frame_at(bounds);
                return;
            }

            using namespace DirectX;

            XMVECTOR scale;
            XMVECTOR rotation_vector;
            XMVECTOR translation;
            const bool valid_transform = XMMatrixDecompose(
                &scale,
                &rotation_vector,
                &translation,
                XMLoadFloat4x4(&source.world_transform));

            const bool valid_lens =
                positive_finite(source.focal_length) &&
                positive_finite(source.vertical_aperture) &&
                positive_finite(source.clipping_range.x) &&
                std::isfinite(source.clipping_range.y) &&
                source.clipping_range.y > source.clipping_range.x;

            if (!valid_transform || !valid_lens) {
                camera.init(
                    {}, {},
                    DirectX::XM_PIDIV4, aspect_ratio,
                    0.01f, 100.0f,
                    1.0f, 0.04f);
                camera.frame_at(bounds);
                return;
            }

            XMFLOAT3 position{};
            XMFLOAT4 rotation{};
            XMStoreFloat3(&position, translation);
            XMStoreFloat4(&rotation, rotation_vector);

            const float vertical_fov =
                2.0f * std::atan(
                    0.5f * source.vertical_aperture /
                    source.focal_length);

            camera.init(
                position,
                rotation,
                vertical_fov,
                aspect_ratio,
                source.clipping_range.x,
                source.clipping_range.y,
                1.0f,
                0.04f);
        }

    } // namespace


    void RendererMain::reset() {
        command_queue_.flush();
        dx::HeapManager::g_heap_manager.reset();
    }

    void RendererMain::init(
        void* window,
        std::uint32_t width,
        std::uint32_t height,
        const scene::StaticScene& scene,
        const RendererOptions& options) {

        const HWND hwnd = static_cast<HWND>(window);
        options_ = options;
        environment_light_ = scene.environment_light;

        // basic resources

        const auto factory = dx::DeviceUtils::create_factory();
        device_ = dx::DeviceUtils::create_device(factory.Get());

        command_queue_.init(
            device_.Get(),
            D3D12_COMMAND_LIST_TYPE_DIRECT);

        swap_chain_.init(
            factory.Get(),
            command_queue_.get_command_queue(),
            hwnd,
            width,
            height,
            FRAME_COUNT,
            options_.vsync);

        auto& descriptor_heaps = dx::HeapManager::g_heap_manager;
        descriptor_heaps.init(
            device_.Get(),
            1024,
            128,
            1,
            FRAME_COUNT);

        desc_rtv_ = descriptor_heaps.heap_rtv.alloc(FRAME_COUNT);
        desc_dsv_ = descriptor_heaps.heap_dsv.alloc();

        for (std::uint32_t frame = 0; frame < FRAME_COUNT; ++frame) {
            command_contexts_[frame].init(
                device_.Get(),
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                frame);
        }

        // scene

        const auto point_culling = PointCullingDataBuilder::build(scene);
        const auto bounds = SceneBoundsBuilder::build(scene, point_culling);

        scene_resources_temp_ = std::make_unique<data::SceneResourcesTemp>(
            SceneResourcesTempBuilder::build(scene, bounds, options));

        SceneResourcesBuilder::Context build_context;
        build_context.device = device_.Get();
        build_context.command_queue = &command_queue_;
        build_context.command_lists = {
            &command_contexts_[0],
            &command_contexts_[1],
        };

        scene_resources_ = std::make_unique<data::SceneResources>(
                SceneResourcesBuilder::build(
                    build_context, scene, *scene_resources_temp_, point_culling.instance_order));

        // remove category by option
        std::erase_if(scene_resources_temp_->draw_items, [&](const data::DrawFinalGPUIndirect& draw) {

            if (draw.instnace_class == data::EnumPointOrMatrix::POINT) {
                if (draw.point_category == scene::StaticScene::EnumPointCategory::RIVER_SEEDLING)
                    return !options_.objects.river_seedling;
                if (draw.point_category == scene::StaticScene::EnumPointCategory::RIVER_FOREST)
                    return !options_.objects.river_forest;
                if (draw.point_category == scene::StaticScene::EnumPointCategory::PYRAMID_MOSS)
                    return !options_.objects.pyramid_moss;
                return !options_.objects.other_foliage;
            }

            const bool is_terrain =
                scene.components.terrain.extended.contains(draw.constants.offset_instance) ||
                scene.components.terrain.cinematic.contains(draw.constants.offset_instance);

            return is_terrain ? !options_.objects.terrain : !options_.objects.other;
            });


        initialize_camera(camera, scene.camera, bounds.world_bounds, width, height, options.frame_entire_scene);

        for (auto& frame : frame_const_data_) {
            SceneFrameConstDataBuilder::build(
                frame,
                device_.Get(),
                camera,
                environment_light_);
        }

        // frame resources

        SceneDynamicDataBuilder::build(
            dynamic_scene_data_,
            *scene_resources_temp_,
            camera,
            options_.lod_selection,
            swap_chain_.get_height());

        forward_pass_.init(
            device_.Get(),
            scene_resources_->materials
                .texture_descriptors.get_count(),
            scene_resources_->materials
                .sampler_descriptors.get_count());

        auto& pass_views = forward_pass_.views;
        pass_views.view_vertices =
            scene_resources_->geometry.vertex_view;
        pass_views.view_indices =
            scene_resources_->geometry.index_view;

        if (scene_resources_->instances.matrix_draw_constants) {
            pass_views.cbuf_transform_matrix = dx::CBufferArrayView{
                scene_resources_->instances.matrix_draw_constants
                    ->GetGPUVirtualAddress(),
                data::Consts::CBUF_ALIGN
            };
        }

        if (scene_resources_->instances.point_draw_constants) {
            pass_views.cbuf_transform_point = dx::CBufferArrayView{
                scene_resources_->instances.point_draw_constants
                    ->GetGPUVirtualAddress(),
                data::Consts::CBUF_ALIGN
            };
        }

        if (scene_resources_->instances.matrix_instances) {
            pass_views.desc_instnaces_matrix =
                scene_resources_->instances.matrix_instances
                    ->GetGPUVirtualAddress();
        }

        if (scene_resources_->instances.point_instances) {
            pass_views.desc_instances_point =
                scene_resources_->instances.point_instances
                    ->GetGPUVirtualAddress();
        }

        if (scene_resources_->materials.materials) {
            pass_views.desc_materials =
                scene_resources_->materials.materials
                    ->GetGPUVirtualAddress();
        }

        if (scene_resources_->materials.texture_bindings) {
            pass_views.desc_texture_bindings =
                scene_resources_->materials.texture_bindings
                    ->GetGPUVirtualAddress();
        }

        pass_views.descs_textures =
            scene_resources_->materials.texture_descriptors;
        pass_views.descs_samplers =
            scene_resources_->materials.sampler_descriptors;

        create_size_dependent_resources(width, height);
    }

    void RendererMain::resize(
        std::uint32_t width,
        std::uint32_t height) {

        command_queue_.flush();
        swap_chain_.resize(width, height);
        create_size_dependent_resources(width, height);
    }

    void RendererMain::create_size_dependent_resources(
        std::uint32_t width,
        std::uint32_t height) {

        camera.set_aspect_ratio(
            static_cast<float>(width) /
            static_cast<float>(height));

        for (std::uint32_t frame = 0;
            frame < FRAME_COUNT;
            ++frame) {

            swap_chain_.get_buffer(frame).create_rtv(
                device_.Get(),
                desc_rtv_.get_cpu(frame),
                0,
                0,
                1,
                DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
        }

        buffer_depth_.reset();

        constexpr DXGI_FORMAT DEPTH_FORMAT =
            DXGI_FORMAT_D32_FLOAT;

        D3D12_RESOURCE_DESC depth_description{};
        depth_description.Dimension =
            D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depth_description.Width = width;
        depth_description.Height = height;
        depth_description.DepthOrArraySize = 1;
        depth_description.MipLevels = 1;
        depth_description.Format = DEPTH_FORMAT;
        depth_description.SampleDesc.Count = 1;
        depth_description.Layout =
            D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depth_description.Flags =
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clear_value{};
        clear_value.Format = DEPTH_FORMAT;
        clear_value.DepthStencil.Depth = 1.0f;

        buffer_depth_.init(
            device_.Get(),
            depth_description,
            dx::TextureType::texture2d,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clear_value);

        buffer_depth_.create_dsv(
            device_.Get(),
            desc_dsv_.get_cpu(),
            0,
            0,
            1,
            DEPTH_FORMAT,
            D3D12_DSV_FLAG_NONE);

        forward_pass_.views.desc_dsv =
            desc_dsv_.get_cpu();
        forward_pass_.views.width = width;
        forward_pass_.views.height = height;
    }

    void RendererMain::render() {

        const std::uint32_t frame =
            swap_chain_.get_current_frame();

        auto& context = command_contexts_[frame];
        command_queue_.wait(
            context.get_fence_value());

        SceneDynamicDataBuilder::build(
            dynamic_scene_data_,
            *scene_resources_temp_,
            camera,
            options_.lod_selection,
            swap_chain_.get_height());

        SceneFrameConstDataBuilder::build(
            frame_const_data_[frame],
            device_.Get(),
            camera,
            environment_light_);

        context.reset();

        swap_chain_.get_current_buffer().transition(
            context.get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        context.SetDescriptorHeaps(
            dx::HeapManager::g_heap_manager
                .heap_sampler.get(),
            dx::HeapManager::g_heap_manager
                .heap_srv_cbv_uav.get());

        forward_pass_.views.desc_rtv =
            desc_rtv_.get_cpu(frame);
        forward_pass_.views.cbuf_camera =
            frame_const_data_[frame]
                .camera_constants.get_address();

        forward_pass_.record(
            context,
            std::span<const data::DrawFinalCPU>{
                dynamic_scene_data_.visible_draws});

        swap_chain_.get_current_buffer().transition(
            context.get(),
            D3D12_RESOURCE_STATE_PRESENT);

        context.close();
        command_queue_.execute(context.get());
        context.set_fence_value(
            command_queue_.signal());
        swap_chain_.present();
    }

} // namespace fjr::render
