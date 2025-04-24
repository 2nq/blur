#include "gui.h"
#include "renderer.h"
#include "sdl.h"
#include "ui/keys.h"
#include "ui/ui.h"

#define DEBUG_RENDER 1

namespace {
	const int PAD_X = 24;
	const int PAD_Y = PAD_X;
}

int gui::run() {
	try {
		sdl::initialise();

		SDL_Event event;

		bool rendered_last = false;

		while (true) {
			auto frame_start = std::chrono::steady_clock::now();

			sdl::update_vsync();

			while (true) {
				if (!SDL_PollEvent(&event))
					break;

				switch (event.type) {
					case SDL_EVENT_QUIT:
						sdl::cleanup();
						return 0;

					case SDL_EVENT_WINDOW_EXPOSED:
						to_render = true;
						break;
				}

				if (keys::process_event(event)) {
					ui::on_update_input_start();

					to_render |= ui::update_container_input(renderer::notification_container);
					to_render |= ui::update_container_input(renderer::nav_container);

					to_render |= ui::update_container_input(renderer::main_container);
					to_render |= ui::update_container_input(renderer::config_container);
					to_render |= ui::update_container_input(renderer::option_information_container);
					to_render |= ui::update_container_input(renderer::config_preview_container);

					ui::on_update_input_end();
				}
			}

			const bool rendered = renderer::redraw_window(
				to_render
			); // note: rendered isn't true if rendering was forced, it's only if an animation or smth is playing

#if DEBUG_RENDER_LOGGING
			u::log("rendered: {}, to render: {}", rendered, to_render);
#endif

			// vsync
			if (rendered || to_render) {
				to_render = false;
				rendered_last = true;

				auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
									  std::chrono::steady_clock::now() - frame_start
				)
				                      .count();

				double time_to_sleep = sdl::vsync_frame_time - elapsed_ms;
				if (time_to_sleep > 0.f)
					SDL_Delay(time_to_sleep);
			}
			else {
				rendered_last = false;
			}
		}
	}
	catch (const std::exception& e) {
		u::log_error("Error: {}", e.what());
		sdl::cleanup();
		return 1;
	}
}
