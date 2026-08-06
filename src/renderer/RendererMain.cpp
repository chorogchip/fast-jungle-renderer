#include "FastJungle/renderer/RendererMain.hpp"

#include <DirectXMath.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <utility>

#include "FastJungle/core/util/Logger.hpp"
#include "FastJungle/renderer/builder/SceneBatchBuilder.hpp"
#include "FastJungle/renderer/builder/SceneBoundsBuilder.hpp"
#include "FastJungle/renderer/builder/SceneDrawBuilder.hpp"
#include "FastJungle/renderer/builder/SceneDynamicDataBuilder.hpp"
#include "FastJungle/renderer/builder/SceneFrameConstDataBuilder.hpp"
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
    void RendererMain::init(
        void* window,
        std::uint32_t width,
        std::uint32_t height,
        const scene::StaticScene& scene,
        const RendererOptions& options) {

        options_ = options;
        environment_light_ = scene.environment_light;

        RendererBase::init(
            window,
            width,
            height,
            options_.vsync);

        // scene

        const auto point_culling = SceneBatchBuilder::build(scene);
        const auto bounds = SceneBoundsBuilder::build(scene, point_culling);

        scene_draws_ = SceneDrawBuilder::build(scene, bounds, options);
        const auto scene_resources_temp =
            SceneResourcesTempBuilder::build(
                scene,
                bounds,
                scene_draws_);

        SceneResourcesBuilder::Context build_context;
        build_context.device = device_.Get();
        build_context.command_queue = &command_queue_;
        build_context.heap_srv_cbv_uav = &heap_srv_cbv_uav_;
        build_context.heap_sampler = &heap_sampler_;
        build_context.command_lists = {
            &command_contexts_[0],
            &command_contexts_[1],
        };

        scene_resources_ = std::make_unique<data::SceneResources>(
            SceneResourcesBuilder::build(
                build_context,
                scene,
                scene_resources_temp,
                point_culling.instance_order));

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
            scene_draws_,
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

        forward_pass_.views.desc_dsv = desc_dsv_.get_cpu();
        forward_pass_.views.width = width;
        forward_pass_.views.height = height;
    }

    void RendererMain::resize(
        std::uint32_t width,
        std::uint32_t height) {

        RendererBase::resize(width, height);
        camera.set_aspect_ratio(
            static_cast<float>(width) /
            static_cast<float>(height));
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
            scene_draws_,
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
            heap_sampler_.get(),
            heap_srv_cbv_uav_.get());

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
