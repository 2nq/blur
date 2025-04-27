#pragma once

namespace sdl {
	inline SDL_Window* window = nullptr;
	inline SDL_GLContext gl_context = nullptr;

	inline const float VSYNC_EXTRA_FPS = 50;
	inline const float MIN_DELTA_TIME = 1.f / 10;
	inline const float DEFAULT_DELTA_TIME = 1.f / 60;
	inline double vsync_frame_time = DEFAULT_DELTA_TIME * 1000.f;

	void initialise();
	void cleanup();

	bool event_watcher(void* data, SDL_Event* event);

	void on_frame_start();
	void set_cursor(SDL_SystemCursor cursor);
	void update_vsync();
}
