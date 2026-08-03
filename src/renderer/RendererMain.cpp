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