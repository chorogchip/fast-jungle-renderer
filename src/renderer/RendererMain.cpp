#include "FastJungle/renderer/RendererMain.hpp"

namespace fjr {

	void RendererMain::init(
		void* window,
		uint32_t width,
		uint32_t height,
		const scene::StaticScene& scene) {

		(void)window;
		(void)width;
		(void)height;
		(void)scene;

		HWND hwnd = static_cast<HWND>(window);

		factory_ = dx::DeviceUtils::create_factory();
		device_ = dx::DeviceUtils::create_device(factory_.Get());
		command_queue.init(device_.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
		swap_chain_.init(
			factory_.Get(),
			command_queue.get_command_queue(),
			hwnd,
			width,
			height,
			FRAME_COUNT,
			false);
	}

	void RendererMain::resize(
		uint32_t width,
		uint32_t height) {

		(void)width;
		(void)height;
	}

	void RendererMain::render() {

	}

	void RendererMain::close() {

	}
}