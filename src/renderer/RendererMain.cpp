#include "FastJungle/renderer/RendererMain.hpp"

#include "FastJungle/dx12/WindowsUtils.hpp"
#include "FastJungle/renderer/SceneResourcesBuilder.hpp"
#include "FastJungle/scene/StaticSceneValidation.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fjr {

    namespace {

        constexpr DXGI_FORMAT COLOR_FORMAT =
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        constexpr DXGI_FORMAT DEPTH_FORMAT =
            DXGI_FORMAT_D32_FLOAT;
        constexpr DXGI_FORMAT VISIBILITY_FORMAT =
            DXGI_FORMAT_R32G32_UINT;

        [[nodiscard]]
        D3D12_GPU_VIRTUAL_ADDRESS gpu_address(
            const dx::Buffer& buffer) noexcept {
            return buffer
                ? buffer->GetGPUVirtualAddress()
                : 0;
        }

    } // namespace

    void RendererMain::init(
        void* window,
        std::uint32_t width,
        std::uint32_t height,
        const scene::StaticScene& scene) {

        scene::validate_static_scene(scene);

        if (command_queue_) {
            close();
        }

        const HWND hwnd = static_cast<HWND>(window);

        factory_ = dx::DeviceUtils::create_factory();
        device_ = dx::DeviceUtils::create_device(factory_.Get());
        command_queue_.init(
            device_.Get(),
            D3D12_COMMAND_LIST_TYPE_DIRECT);
        swap_chain_.init(
            factory_.Get(),
            command_queue_.get_command_queue(),
            hwnd,
            width,
            height,
            FRAME_COUNT,
            false);

        heap_rtv_.init(
            device_.Get(),
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            FRAME_COUNT + 1,
            false);
        heap_dsv_.init(
            device_.Get(),
            D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            1,
            false);
        heap_visibility_srv_.init(
            device_.Get(),
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            1,
            true);

        for (auto& context : command_contexts_) {
            context.init(
                device_.Get(),
                D3D12_COMMAND_LIST_TYPE_DIRECT);
        }

        auto& upload_context = command_contexts_[0];
        upload_context.reset();

        render::SceneResourcesBuilder resources_builder;
        scene_resources_ = resources_builder.build(
            device_.Get(),
            upload_context.get(),
            FRAME_COUNT,
            scene);

        camera_.set_scene_camera(scene.camera);
        camera_.set_viewport(width, height);
        if (!camera_.has_valid_lens() ||
            !camera_.has_valid_transform()) {
            camera_.frame_bounds(scene.info.world_bounds);
        }

        create_render_target_views();
        create_depth_buffer(width, height);
        create_visibility_buffer(width, height);
        create_passes();

        for (std::uint32_t frame = 0;
            frame < FRAME_COUNT;
            ++frame) {
            update_camera_constants(frame);
        }

        upload_context.close();
        command_queue_.execute(upload_context.get());
        command_queue_.flush();
        scene_resources_->upload_buffers = {};
        scene_resources_->point_draw_constants = {};
        scene_resources_->matrix_draw_constants = {};
        scene_resources_->material_data = {};
        scene_resources_->texture_binding_data = {};
        scene_resources_->draw_data = {};
        frame_fence_values_.fill(0);
    }

    void RendererMain::create_render_target_views() {
        D3D12_RENDER_TARGET_VIEW_DESC description{};
        description.Format = COLOR_FORMAT;
        description.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        for (std::uint32_t frame = 0;
            frame < FRAME_COUNT;
            ++frame) {
            device_->CreateRenderTargetView(
                swap_chain_.get_buffer(frame).get(),
                &description,
                heap_rtv_.get_cpu_handle(frame));
        }
    }

    void RendererMain::create_depth_buffer(
        std::uint32_t width,
        std::uint32_t height) {

        depth_buffer_.reset();

        D3D12_RESOURCE_DESC description{};
        description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        description.Width = width;
        description.Height = height;
        description.DepthOrArraySize = 1;
        description.MipLevels = 1;
        description.Format = DEPTH_FORMAT;
        description.SampleDesc.Count = 1;
        description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        description.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clear_value{};
        clear_value.Format = DEPTH_FORMAT;
        clear_value.DepthStencil.Depth = 1.0f;
        clear_value.DepthStencil.Stencil = 0;

        depth_buffer_.init(
            device_.Get(),
            description,
            dx::TextureType::texture2d,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clear_value);

        D3D12_DEPTH_STENCIL_VIEW_DESC view{};
        view.Format = DEPTH_FORMAT;
        view.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        device_->CreateDepthStencilView(
            depth_buffer_.get(),
            &view,
            heap_dsv_.get_cpu_handle(0));
    }

    void RendererMain::create_visibility_buffer(
        std::uint32_t width,
        std::uint32_t height) {

        visibility_buffer_.reset();

        D3D12_RESOURCE_DESC description{};
        description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        description.Width = width;
        description.Height = height;
        description.DepthOrArraySize = 1;
        description.MipLevels = 1;
        description.Format = VISIBILITY_FORMAT;
        description.SampleDesc.Count = 1;
        description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        description.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE clear_value{};
        clear_value.Format = VISIBILITY_FORMAT;

        visibility_buffer_.init(
            device_.Get(),
            description,
            dx::TextureType::texture2d,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clear_value);

        D3D12_RENDER_TARGET_VIEW_DESC render_target_view{};
        render_target_view.Format = VISIBILITY_FORMAT;
        render_target_view.ViewDimension =
            D3D12_RTV_DIMENSION_TEXTURE2D;
        device_->CreateRenderTargetView(
            visibility_buffer_.get(),
            &render_target_view,
            heap_rtv_.get_cpu_handle(VISIBILITY_RTV_INDEX));

        D3D12_SHADER_RESOURCE_VIEW_DESC shader_resource_view{};
        shader_resource_view.Format = VISIBILITY_FORMAT;
        shader_resource_view.ViewDimension =
            D3D12_SRV_DIMENSION_TEXTURE2D;
        shader_resource_view.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        shader_resource_view.Texture2D.MipLevels = 1;
        device_->CreateShaderResourceView(
            visibility_buffer_.get(),
            &shader_resource_view,
            heap_visibility_srv_.get_cpu_handle(0));
    }

    void RendererMain::create_passes() {
        forward_pass_.init(
            device_.Get(),
            COLOR_FORMAT,
            DEPTH_FORMAT,
            scene_resources_->heap_srv.get_capacity(),
            scene_resources_->heap_samplers.get_capacity());
        visibility_pass_.init(
            device_.Get(),
            VISIBILITY_FORMAT,
            DEPTH_FORMAT);
        visibility_resolve_pass_.init(
            device_.Get(),
            COLOR_FORMAT);
    }

    void RendererMain::update_camera_constants(
        std::uint32_t frame_index) {

        render::SceneResources::CameraConstants constants;
        constants.view_projection = camera_.get_view_projection();
        constants.world_position = camera_.get_world_position();

        constants.environment_world_transform =
            scene_resources_->environment_light.world_transform;
        constants.environment_color =
            scene_resources_->environment_light.color;
        constants.environment_intensity =
            scene_resources_->environment_light.intensity *
            std::exp2(scene_resources_->environment_light.exposure);
        constants.environment_texture_id =
            scene_resources_->environment_light.texture;

        void* mapped = nullptr;
        const D3D12_RANGE read_range{0, 0};
        dx::abort_failed(scene_resources_->buf_cbuffer_camera->Map(
            0,
            &read_range,
            &mapped));

        const std::size_t byte_offset =
            static_cast<std::size_t>(frame_index) * sizeof(constants);
        std::memcpy(
            static_cast<std::byte*>(mapped) + byte_offset,
            &constants,
            sizeof(constants));
        const D3D12_RANGE written_range{
            byte_offset,
            byte_offset + sizeof(constants),
        };
        scene_resources_->buf_cbuffer_camera->Unmap(
            0,
            &written_range);
    }

    render::ForwardPassView RendererMain::make_forward_view(
        std::uint32_t frame_index) const noexcept {

        render::ForwardPassView view;
        view.render_target = heap_rtv_.get_cpu_handle(frame_index);
        view.depth_stencil = heap_dsv_.get_cpu_handle(0);
        view.width = swap_chain_.get_width();
        view.height = swap_chain_.get_height();
        view.camera_constants =
            gpu_address(scene_resources_->buf_cbuffer_camera) +
            static_cast<UINT64>(frame_index) *
            sizeof(render::SceneResources::CameraConstants);
        view.point_transform_constants =
            gpu_address(scene_resources_->buf_cbuffer_point);
        view.matrix_transform_constants =
            gpu_address(scene_resources_->buf_cbuffer_matrix);
        view.point_instances =
            gpu_address(scene_resources_->buf_instances_point);
        view.matrix_instances =
            gpu_address(scene_resources_->buf_instances_matrix);
        view.materials = gpu_address(scene_resources_->buf_materials);
        view.texture_bindings =
            gpu_address(scene_resources_->buf_texture_bindings);
        view.textures = scene_resources_->heap_srv.get_gpu_start();
        view.samplers = scene_resources_->heap_samplers.get_gpu_start();
        view.vertices = scene_resources_->view_vertices;
        view.indices = scene_resources_->view_indices;
        view.draws = scene_resources_->draw_items;
        return view;
    }

    render::VisibilityPassView RendererMain::make_visibility_view(
        std::uint32_t frame_index) const noexcept {

        render::VisibilityPassView view;
        view.render_target =
            heap_rtv_.get_cpu_handle(VISIBILITY_RTV_INDEX);
        view.depth_stencil = heap_dsv_.get_cpu_handle(0);
        view.width = swap_chain_.get_width();
        view.height = swap_chain_.get_height();
        view.camera_constants =
            gpu_address(scene_resources_->buf_cbuffer_camera) +
            static_cast<UINT64>(frame_index) *
            sizeof(render::SceneResources::CameraConstants);
        view.point_transform_constants =
            gpu_address(scene_resources_->buf_cbuffer_point);
        view.matrix_transform_constants =
            gpu_address(scene_resources_->buf_cbuffer_matrix);
        view.point_instances =
            gpu_address(scene_resources_->buf_instances_point);
        view.matrix_instances =
            gpu_address(scene_resources_->buf_instances_matrix);
        view.vertices = scene_resources_->view_vertices;
        view.indices = scene_resources_->view_indices;
        view.draws = scene_resources_->draw_items;
        return view;
    }

    render::VisibilityResolvePassView
        RendererMain::make_visibility_resolve_view(
            std::uint32_t frame_index) const noexcept {

        render::VisibilityResolvePassView view;
        view.render_target = heap_rtv_.get_cpu_handle(frame_index);
        view.width = swap_chain_.get_width();
        view.height = swap_chain_.get_height();
        view.visibility = heap_visibility_srv_.get_gpu_handle(0);
        view.draws = gpu_address(scene_resources_->buf_draw_data);
        view.materials = gpu_address(scene_resources_->buf_materials);
        return view;
    }

    void RendererMain::record_forward(
        dx::CommandContext& context,
        std::uint32_t frame_index) {

        auto* command_list = context.get();
        auto& back_buffer = swap_chain_.get_current_buffer();

        back_buffer.transition(
            command_list,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        depth_buffer_.transition(
            command_list,
            D3D12_RESOURCE_STATE_DEPTH_WRITE);

        context.SetDescriptorHeaps(
            scene_resources_->heap_srv.get_descriptor_heap(),
            scene_resources_->heap_samplers.get_descriptor_heap());
        forward_pass_.record(
            command_list,
            make_forward_view(frame_index));

        back_buffer.transition(
            command_list,
            D3D12_RESOURCE_STATE_PRESENT);
    }

    void RendererMain::record_visibility_buffer(
        dx::CommandContext& context,
        std::uint32_t frame_index) {

        auto* command_list = context.get();
        auto& back_buffer = swap_chain_.get_current_buffer();

        visibility_buffer_.transition(
            command_list,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        depth_buffer_.transition(
            command_list,
            D3D12_RESOURCE_STATE_DEPTH_WRITE);
        visibility_pass_.record(
            command_list,
            make_visibility_view(frame_index));

        visibility_buffer_.transition(
            command_list,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        back_buffer.transition(
            command_list,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        context.SetDescriptorHeaps(
            heap_visibility_srv_.get_descriptor_heap());
        visibility_resolve_pass_.record(
            command_list,
            make_visibility_resolve_view(frame_index));

        back_buffer.transition(
            command_list,
            D3D12_RESOURCE_STATE_PRESENT);
    }

    void RendererMain::resize(
        std::uint32_t width,
        std::uint32_t height) {

        command_queue_.flush();
        swap_chain_.resize(width, height);
        camera_.set_viewport(width, height);
        create_render_target_views();
        create_depth_buffer(width, height);
        create_visibility_buffer(width, height);

        for (std::uint32_t frame = 0;
            frame < FRAME_COUNT;
            ++frame) {
            update_camera_constants(frame);
        }
        frame_fence_values_.fill(0);
    }

    void RendererMain::render() {
        if (!scene_resources_) {
            return;
        }

        const std::uint32_t frame = swap_chain_.get_current_frame();
        if (frame_fence_values_[frame] != 0) {
            command_queue_.wait(frame_fence_values_[frame]);
        }

        update_camera_constants(frame);

        auto& context = command_contexts_[frame];
        context.reset();

        switch (render_path_) {
        case RenderPath::FORWARD:
            record_forward(context, frame);
            break;
        case RenderPath::VISIBILITY_BUFFER:
            record_visibility_buffer(context, frame);
            break;
        }

        context.close();
        command_queue_.execute(context.get());
        swap_chain_.present();
        frame_fence_values_[frame] = command_queue_.signal();
    }

    void RendererMain::close() {
        if (command_queue_) {
            command_queue_.flush();
        }

        scene_resources_.reset();
        visibility_buffer_.reset();
        depth_buffer_.reset();
        forward_pass_.reset();
        visibility_pass_.reset();
        visibility_resolve_pass_.reset();

        heap_visibility_srv_ = {};
        heap_rtv_ = {};
        heap_dsv_ = {};
        swap_chain_ = {};
        for (auto& context : command_contexts_) {
            context = {};
        }
        command_queue_ = {};
        camera_ = {};
        device_.Reset();
        factory_.Reset();
        frame_fence_values_.fill(0);
    }

} // namespace fjr
