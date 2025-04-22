#pragma once

#include "os/skia/skia_window.h"

#include <mpv/client.h>
#include <mpv/render_gl.h>

class VideoPlayer {
public:
	VideoPlayer(os::SkiaWindow* skia_window);
	~VideoPlayer();

	bool initialise(os::SkiaWindow* skia_window);
	bool load_file(const std::string& path);
	void render(int width, int height);
	void handle_events();
	void toggle_pause();
	void shutdown();

	// Set this callback to be notified when a new frame is ready
	void set_render_update_callback(std::function<void()> callback);

	// Set this callback to be notified when mpv events occur
	void set_event_callback(std::function<void()> callback);

private:
	mpv_handle* m_mpv = nullptr;
	mpv_render_context* m_mpv_gl = nullptr;

	std::function<void()> m_render_update_callback;
	std::function<void()> m_event_callback;

	static void* get_proc_address_mpv(void* fn_ctx, const char* name);
	static void on_mpv_render_update(void* ctx);
	static void on_mpv_events(void* ctx);
};
