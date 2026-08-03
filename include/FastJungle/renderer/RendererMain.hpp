#pragma once

#include <cstdint>
#include <array>

#include "FastJungle/dx12/DeviceUtils.hpp"
#include "FastJungle/dx12/SwapChain.hpp"
#include "FastJungle/dx12/CommandQueue.hpp"
#include "FastJungle/dx12/CommandContext.hpp"
#include "FastJungle/dx12/Shader.hpp"
#include "FastJungle/dx12/RootSignatureBuilder.hpp"
#include "FastJungle/dx12/PSOUtils.hpp"
#include "FastJungle/dx12/Buffer.hpp"
#include "FastJungle/dx12/Texture.hpp"
#include "FastJungle/scene/StaticScene.hpp"

namespace fjr {

	class RendererMain {

	public:
		RendererMain() = default;
		~RendererMain() = default;

		RendererMain(const RendererMain&) = delete;
		RendererMain(RendererMain&&) = delete;
		RendererMain& operator=(const RendererMain&) = delete;
		RendererMain& operator=(RendererMain&&) = delete;

		void init(
			void* window,
			uint32_t width,
			uint32_t height,
			const scene::StaticScene& scene);

		void resize(
			uint32_t width,
			uint32_t height);

		void render();

		void close();
		
	private:
		static constexpr uint32_t FRAME_COUNT = 2;

		Microsoft::WRL::ComPtr<IDXGIFactory4> factory_;
		Microsoft::WRL::ComPtr<ID3D12Device> device_;

		dx::SwapChain swap_chain_;
		dx::CommandQueue command_queue;
		std::array<dx::CommandContext, 2> command_contexts_;
		dx::DescriptorHeap heap_rtv_;
		dx::DescriptorHeap heap_dsv_;
		dx::Texture depth_buffer_;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_state_;
	};


}
