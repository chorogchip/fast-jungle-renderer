#include "FastJungle/renderer/RendererMain.hpp"

#include "FastJungle/dx12/WindowsUtils.hpp"
#include "FastJungle/dx12/HeapManager.hpp"
#include "FastJungle/renderer/SceneResourcesBuilder.hpp"
#include "FastJungle/scene/StaticSceneValidation.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fjr::render {

    RendererMain::~RendererMain() {
        command_queue_.flush();
    }

    void RendererMain::init(
        void* window,
        std::uint32_t width,
        std::uint32_t height,
        const scene::StaticScene& scene) {

        // basic resource

        const HWND hwnd = static_cast<HWND>(window);

        factory_ = dx::DeviceUtils::create_factory();
        device_ = dx::DeviceUtils::create_device(factory_.Get());

        command_queue_.init(
            device_.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);

        swap_chain_.init(
            factory_.Get(), command_queue_.get_command_queue(),
            hwnd, width, height, FRAME_COUNT, false);

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

        dx::CommandContext upload_context;
        upload_context.init(
            device_.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, 0);
        upload_context.reset();

        SceneResourcesBuilder::BuildContexts build_contexts{};
        build_contexts.device = device_.Get();
        build_contexts.context = upload_context.get();
        scene_resources_ = render::SceneResourcesBuilder::build(
            build_contexts, scene);
        upload_context.close();
        command_queue_.execute(upload_context.get());
        command_queue_.flush();

        scene_viewer_.init(&scene, scene_resources_.get());

        // camera
        camera_.set_scene_camera(scene.camera);
        camera_.set_viewport(width, height);
        if (!camera_.has_valid_lens() || !camera_.has_valid_transform()) {
            camera_.frame_bounds(scene.info.world_bounds);
        }

        forward_pass_.init(
            device_.Get(),
            scene_resources_->texture_descriptors.get_count(),
            scene_resources_->sampler_descriptors.get_count());

        this->resize(width, height);
    }

    void RendererMain::resize(
        std::uint32_t width,
        std::uint32_t height) {

        command_queue_.flush();
        swap_chain_.resize(width, height);
        camera_.set_viewport(width, height);

        // Render Target

        D3D12_RENDER_TARGET_VIEW_DESC desc_rtv{};
        desc_rtv.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        desc_rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        for (uint32_t i = 0; i < FRAME_COUNT; ++i) {
            device_->CreateRenderTargetView(
                swap_chain_.get_buffer(i).get(),
                &desc_rtv,
                desc_rtv_.get_cpu(i));
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

        D3D12_DEPTH_STENCIL_VIEW_DESC view{};
        view.Format = DEPTH_FORMAT;
        view.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

        device_->CreateDepthStencilView(
            buffer_depth_.get(),
            &view,
            desc_dsv_.get_cpu());
        forward_pass_.views.view_vertices = scene_resources_->view_vertices;
        forward_pass_.views.view_indices = scene_resources_->view_indices;
        forward_pass_.views.desc_dsv = desc_dsv_.get_cpu();

        forward_pass_.views.cbuf_transform_matrix =
            scene_resources_->view_cbuf_transform_matrix;
        forward_pass_.views.cbuf_transform_point =
            scene_resources_->view_cbuf_transform_point;

        forward_pass_.views.desc_instnaces_matrix = 
            scene_resources_->buf_instances_matrix->GetGPUVirtualAddress();
        forward_pass_.views.desc_instances_point =
            scene_resources_->buf_instances_point->GetGPUVirtualAddress();

        forward_pass_.views.desc_materials =
            scene_resources_->buf_materials->GetGPUVirtualAddress();
        forward_pass_.views.desc_texture_bindings =
            scene_resources_->buf_texture_bindings->GetGPUVirtualAddress();

        forward_pass_.views.descs_textures =
            scene_resources_->texture_descriptors;
        forward_pass_.views.descs_samplers = 
            scene_resources_->sampler_descriptors;

        forward_pass_.views.width = width;
        forward_pass_.views.height = height;

        for (std::uint32_t i = 0; i < FRAME_COUNT; ++i)
            frame_data_[i].upload_camera_data(camera_, scene_resources_->environment_light);
    }

    void RendererMain::render() {

        const int frame = swap_chain_.get_current_frame();


        auto& context = command_contexts_[frame];
        command_queue_.wait(context.get_fence_value());
        frame_data_[frame].upload_camera_data(camera_, scene_resources_->environment_light);
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

} // namespace fjr
