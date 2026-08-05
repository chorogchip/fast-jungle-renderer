#include "FastJungle/renderer/RendererMain.hpp"

#include "FastJungle/dx12/WindowsUtils.hpp"
#include "FastJungle/dx12/HeapManager.hpp"
#include "FastJungle/renderer/builder/SceneBoundsBuilder.hpp"
#include "FastJungle/renderer/builder/SceneResourcesBuilder.hpp"
#include "internal/CameraController.hpp"

#include <cstdint>
#include <utility>

namespace fjr::render {

    RendererMain::RendererMain() = default;

    RendererMain::~RendererMain() {
        command_queue_.flush();
    }

    void RendererMain::init(
        void* window,
        std::uint32_t width,
        std::uint32_t height,
        const scene::StaticScene& scene,
        const RendererOptions& options) {

        // basic resource

        const HWND hwnd = static_cast<HWND>(window);
        options_ = options;
        camera_controller_ =
            std::make_unique<internal::CameraController>(window);

        const auto factory = dx::DeviceUtils::create_factory();
        device_ = dx::DeviceUtils::create_device(factory.Get());

        command_queue_.init(
            device_.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);

        swap_chain_.init(
            factory.Get(), command_queue_.get_command_queue(),
            hwnd, width, height, FRAME_COUNT, true);

        auto& descriptor_heaps = dx::HeapManager::g_heap_manager;
        descriptor_heaps.init(
            device_.Get(),
            1024, 128, 1, FRAME_COUNT);
        desc_rtv_ = descriptor_heaps.heap_rtv.alloc(FRAME_COUNT);
        desc_dsv_ = descriptor_heaps.heap_dsv.alloc();

        for (uint32_t i = 0; i < FRAME_COUNT; ++i) {
            command_contexts_[i].init(
                device_.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, i);
            frame_data_[i].init(device_.Get());
        }

        // build scene

        environment_light_ = scene.environment_light;

        SceneResourcesBuilder::BuildContexts build_contexts{};
        build_contexts.device = device_.Get();
        build_contexts.command_queue = &command_queue_;

        auto scene_build = render::SceneResourcesBuilder::build(
            build_contexts, scene, options);
        scene_resources_ = std::move(scene_build.resources);

        const auto bounds = SceneBoundsBuilder::build(scene);
        scene_viewer_.init(scene_build.draw_items, bounds);

        // camera
        camera_.set_scene_camera(scene.camera);
        camera_.set_viewport(width, height);
        if (options_.frame_entire_scene) {
            camera_.frame_bounds(bounds.world_bounds);
        } else if (!camera_.has_valid_lens() ||
            !camera_.has_valid_transform()) {
            camera_.frame_bounds(bounds.world_bounds);
        }

        forward_pass_.init(
            device_.Get(),
            scene_resources_->texture_descriptors.get_count(),
            scene_resources_->sampler_descriptors.get_count());

        forward_pass_.views.view_vertices = scene_resources_->view_vertices;
        forward_pass_.views.view_indices = scene_resources_->view_indices;
        forward_pass_.views.cbuf_transform_matrix =
            scene_resources_->view_cbuf_transform_matrix;
        forward_pass_.views.cbuf_transform_point =
            scene_resources_->view_cbuf_transform_point;
        if (scene_resources_->buf_instances_matrix) {
            forward_pass_.views.desc_instnaces_matrix =
                scene_resources_->buf_instances_matrix
                    ->GetGPUVirtualAddress();
        }
        if (scene_resources_->buf_instances_point) {
            forward_pass_.views.desc_instances_point =
                scene_resources_->buf_instances_point
                    ->GetGPUVirtualAddress();
        }
        forward_pass_.views.desc_materials =
            scene_resources_->buf_materials->GetGPUVirtualAddress();
        forward_pass_.views.desc_texture_bindings =
            scene_resources_->buf_texture_bindings->GetGPUVirtualAddress();
        forward_pass_.views.descs_textures =
            scene_resources_->texture_descriptors;
        forward_pass_.views.descs_samplers =
            scene_resources_->sampler_descriptors;

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

        camera_.set_viewport(width, height);
        scene_viewer_.update_visibility(camera_, options_.lod_selection);

        // Render Target

        for (uint32_t i = 0; i < FRAME_COUNT; ++i) {
            swap_chain_.get_buffer(i).create_rtv(
                device_.Get(),
                desc_rtv_.get_cpu(i),
                0,
                0,
                1,
                DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
        }

        // Depth Stencil

        buffer_depth_.reset();

        constexpr DXGI_FORMAT DEPTH_FORMAT = DXGI_FORMAT_D32_FLOAT;

        D3D12_RESOURCE_DESC desc_depthbuf{};
        desc_depthbuf.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc_depthbuf.Width = width;
        desc_depthbuf.Height = height;
        desc_depthbuf.DepthOrArraySize = 1;
        desc_depthbuf.MipLevels = 1;
        desc_depthbuf.Format = DEPTH_FORMAT;
        desc_depthbuf.SampleDesc.Count = 1;
        desc_depthbuf.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc_depthbuf.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clear_value{};
        clear_value.Format = DEPTH_FORMAT;
        clear_value.DepthStencil.Depth = 1.0f;
        clear_value.DepthStencil.Stencil = 0;

        buffer_depth_.init(
            device_.Get(),
            desc_depthbuf,
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

        forward_pass_.views.desc_dsv = desc_dsv_.get_cpu();
        forward_pass_.views.width = width;
        forward_pass_.views.height = height;
    }

    void RendererMain::render() {

        camera_controller_->update(camera_);
        scene_viewer_.update_visibility(camera_, options_.lod_selection);

        const int frame = swap_chain_.get_current_frame();
        auto& context = command_contexts_[frame];
        command_queue_.wait(context.get_fence_value());
        frame_data_[frame].upload_camera_data(camera_, environment_light_);
        context.reset();

        swap_chain_.get_current_buffer().transition(
            context.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
        context.SetDescriptorHeaps(
            dx::HeapManager::g_heap_manager.heap_sampler.get(),
            dx::HeapManager::g_heap_manager.heap_srv_cbv_uav.get()
        );

        forward_pass_.views.desc_rtv = desc_rtv_.get_cpu(frame);
        forward_pass_.views.cbuf_camera = frame_data_[frame].get_camera_buffer();
        forward_pass_.record(context, scene_viewer_.get_draw_data());

        swap_chain_.get_current_buffer().transition(
            context.get(), D3D12_RESOURCE_STATE_PRESENT);

        context.close();
        command_queue_.execute(context.get());
        context.set_fence_value(command_queue_.signal());
        swap_chain_.present();
    }

    void RendererMain::handle_key_down(uint32_t virtual_key) {
        camera_controller_->step(camera_, virtual_key, options_.lod_selection);
    }

} // namespace fjr
