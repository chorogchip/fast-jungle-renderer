#include "FastJungle/renderer/Renderer.hpp"

#include <cstdlib>
#include <filesystem>
#include <span>

namespace fjr {

    namespace {

        void abort_if_failed(HRESULT result) {
            if (FAILED(result)) {
                std::abort();
            }
        }

        D3D12_RESOURCE_BARRIER transition(
            ID3D12Resource* resource,
            D3D12_RESOURCE_STATES before,
            D3D12_RESOURCE_STATES after) {

            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = resource;
            barrier.Transition.StateBefore = before;
            barrier.Transition.StateAfter = after;
            barrier.Transition.Subresource =
                D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            return barrier;
        }

    } // namespace

    void Renderer::close() {
        if (command_queue_) {
            command_queue_.flush();
            command_context_.set_fence_value(0);
        }
    }

    void Renderer::init(
        void* native_window,
        std::uint32_t width,
        std::uint32_t height) {

        if (native_window == nullptr || width == 0 || height == 0) {
            std::abort();
        }

        window_ = static_cast<HWND>(native_window);
        width_ = static_cast<UINT>(width);
        height_ = static_cast<UINT>(height);

        factory_ = dx::DeviceUtils::create_factory();
        device_ = dx::DeviceUtils::create_device(factory_.Get());
        command_queue_.init(
            device_.Get(),
            D3D12_COMMAND_LIST_TYPE_DIRECT);
        swap_chain_.init(
            factory_.Get(),
            command_queue_.get_command_queue(),
            window_,
            width_,
            height_,
            FRAME_COUNT,
            true);

        rtv_heap_.init(
            device_.Get(),
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            FRAME_COUNT,
            false);
        for (UINT i = 0; i < FRAME_COUNT; ++i) {
            Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
            abort_if_failed(swap_chain_->GetBuffer(
                i,
                IID_PPV_ARGS(buffer.ReleaseAndGetAddressOf())));
            device_->CreateRenderTargetView(
                buffer.Get(),
                nullptr,
                rtv_heap_.get_cpu_handle(i));
        }

        command_context_.init(
            device_.Get(),
            D3D12_COMMAND_LIST_TYPE_DIRECT);

        dx::RootSignatureBuilder root_builder;
        root_builder.init(0);
        root_builder.set_flags(
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
        root_signature_ = root_builder.build(device_.Get());

        dx::Shader vertex_shader;
        vertex_shader.load(
            std::filesystem::path{FASTJUNGLE_SHADER_OUTPUT_DIR} /
            "Triangle.vs.dxil");

        dx::Shader pixel_shader;
        pixel_shader.load(
            std::filesystem::path{FASTJUNGLE_SHADER_OUTPUT_DIR} /
            "Triangle.ps.dxil");

        auto description = dx::PSOUtils::default_graphics_desc();
        description.pRootSignature = root_signature_.Get();
        description.VS = vertex_shader.get_bytecode();
        description.PS = pixel_shader.get_bytecode();
        description.DepthStencilState.DepthEnable = FALSE;
        description.DepthStencilState.DepthWriteMask =
            D3D12_DEPTH_WRITE_MASK_ZERO;
        description.InputLayout = { nullptr, 0 };
        description.NumRenderTargets = 1;
        description.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pipeline_state_ = dx::PSOUtils::create_graphics(
            device_.Get(),
            description);

        frame_index_ = swap_chain_.get_current_frame();
    }

    void Renderer::resize(
        std::uint32_t width,
        std::uint32_t height) {

        if (width == 0 || height == 0 ||
            (width_ == width && height_ == height)) {
            return;
        }

        command_queue_.flush();
        command_context_.set_fence_value(0);
        swap_chain_.resize(
            static_cast<UINT>(width),
            static_cast<UINT>(height));
        width_ = static_cast<UINT>(width);
        height_ = static_cast<UINT>(height);
        for (UINT i = 0; i < FRAME_COUNT; ++i) {
            Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
            abort_if_failed(swap_chain_->GetBuffer(
                i,
                IID_PPV_ARGS(buffer.ReleaseAndGetAddressOf())));
            device_->CreateRenderTargetView(
                buffer.Get(),
                nullptr,
                rtv_heap_.get_cpu_handle(i));
        }
        frame_index_ = swap_chain_.get_current_frame();
    }

    void Renderer::render() {
        if (command_context_.get_fence_value() != 0) {
            command_queue_.wait(command_context_.get_fence_value());
        }
        command_context_.reset(pipeline_state_.Get());

        const auto to_render_target = transition(
            swap_chain_.get_current_buffer(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        command_context_->ResourceBarrier(1, &to_render_target);

        const auto rtv = rtv_heap_.get_cpu_handle(frame_index_);
        constexpr float clear_color[] = { 0.03f, 0.04f, 0.08f, 1.0f };
        command_context_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        command_context_->ClearRenderTargetView(
            rtv,
            clear_color,
            0,
            nullptr);

        const D3D12_VIEWPORT viewport{
            0.0f,
            0.0f,
            static_cast<float>(width_),
            static_cast<float>(height_),
            0.0f,
            1.0f
        };
        const D3D12_RECT scissor{
            0,
            0,
            static_cast<LONG>(width_),
            static_cast<LONG>(height_)
        };
        command_context_->RSSetViewports(1, &viewport);
        command_context_->RSSetScissorRects(1, &scissor);
        command_context_->SetGraphicsRootSignature(root_signature_.Get());
        command_context_->IASetPrimitiveTopology(
            D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_context_->DrawInstanced(3, 1, 0, 0);

        const auto to_present = transition(
            swap_chain_.get_current_buffer(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);
        command_context_->ResourceBarrier(1, &to_present);
        command_context_.close();

        ID3D12CommandList* command_lists[] = {
            command_context_.get_command_list()
        };
        command_queue_.execute(std::span<ID3D12CommandList* const>{
            command_lists,
            1
        });
        swap_chain_.present();

        command_context_.set_fence_value(command_queue_.signal());
        frame_index_ = swap_chain_.get_current_frame();
    }

} // namespace fjr
