
#include "video.h"

void* VideoPlayer::get_proc_address_mpv(void* fn_ctx, const char* name) {
	// Assuming fn_ctx is a pointer to your GL context
	// Cast it to the appropriate type if needed
	auto* gl_context = static_cast<os::GLContext*>(fn_ctx);

	// Use the context to get the function pointer
	// This will depend on your GL context implementation
	auto* symbol = gl_context->getProcAddress(name);
	return symbol;
}

VideoPlayer::VideoPlayer(os::SkiaWindow* skia_window) {
	initialise(skia_window);
}

VideoPlayer::~VideoPlayer() {
	shutdown();
}

bool VideoPlayer::initialise(os::SkiaWindow* skia_window) {
	if (!skia_window) {
		u::log("Invalid Skia window provided");
		return false;
	}

	// Create MPV instance
	m_mpv = mpv_create();
	if (!m_mpv) {
		u::log("Failed to create MPV context");
		return false;
	}

	// Configure MPV
	mpv_set_option_string(m_mpv, "vo", "libmpv");

	// Initialize MPV
	if (mpv_initialize(m_mpv) < 0) {
		u::log("Failed to initialize MPV");
		mpv_destroy(m_mpv);
		m_mpv = nullptr;
		return false;
	}

	// Request debug log messages
	mpv_request_log_messages(m_mpv, "debug");

	auto& gl_context = skia_window->m_glCtx;

	if (!gl_context || !gl_context->isValid()) {
		// Handle error - context is not valid
		return false;
	}

	// Create the GL init parameters
	mpv_opengl_init_params init_params{
		.get_proc_address = get_proc_address_mpv,
		.get_proc_address_ctx = gl_context.get(),
	};

	int advanced_control = 1;

	std::vector<mpv_render_param> params{ { .type = MPV_RENDER_PARAM_API_TYPE,
		                                    .data = const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL) },
		                                  { .type = MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, .data = &init_params },
		                                  { .type = MPV_RENDER_PARAM_ADVANCED_CONTROL, .data = &advanced_control },
		                                  { .type = MPV_RENDER_PARAM_INVALID } };

	// Create MPV render context
	if (mpv_render_context_create(&m_mpv_gl, m_mpv, params.data()) < 0) {
		u::log("Failed to initialize MPV GL context");
		mpv_destroy(m_mpv);
		m_mpv = nullptr;
		return false;
	}

	// Set up callbacks
	mpv_set_wakeup_callback(m_mpv, on_mpv_events, this);
	mpv_render_context_set_update_callback(m_mpv_gl, on_mpv_render_update, this);

	return true;
}

bool VideoPlayer::load_file(const std::string& path) {
	if (!m_mpv) {
		u::log("MPV not initialized");
		return false;
	}

	// Using std::array for safer command construction
	std::array<const char*, 3> cmd = { { "loadfile", path.c_str(), nullptr } };
	mpv_command_async(m_mpv, 0, cmd.data());
	return true;
}

void VideoPlayer::render(int width, int height) {
	if (!m_mpv_gl)
		return;

	// Update MPV's render context (check if we need to render a new frame)
	uint64_t flags = mpv_render_context_update(m_mpv_gl);

	// Create an fbo struct that persists through the function call
	mpv_opengl_fbo fbo{
		0,      // FBO 0 = default framebuffer
		width,  // Width
		height, // Height
		0       // Internal format (0 = unknown/don't care)
	};

	// Set flip_y parameter
	int flip_y = 1;

	std::vector<mpv_render_param> params{ { .type = MPV_RENDER_PARAM_OPENGL_FBO, .data = &fbo },
		                                  { .type = MPV_RENDER_PARAM_FLIP_Y, .data = &flip_y },
		                                  { .type = MPV_RENDER_PARAM_INVALID } };

	// Render the video frame
	mpv_render_context_render(m_mpv_gl, params.data());
}

void VideoPlayer::handle_events() {
	if (!m_mpv)
		return;

	// Process all MPV events
	while (true) {
		mpv_event* event = mpv_wait_event(m_mpv, 0);
		if (event->event_id == MPV_EVENT_NONE)
			break;

		switch (event->event_id) {
			case MPV_EVENT_LOG_MESSAGE: {
				auto* msg = static_cast<mpv_event_log_message*>(event->data);
				if (msg && msg->text && std::strstr(msg->text, "DR image"))
					u::log("MPV log: {}", msg->text);
				break;
			}
			default:
				u::log("MPV event: {}", mpv_event_name(event->event_id));
				break;
		}
	}
}

void VideoPlayer::toggle_pause() {
	if (!m_mpv)
		return;

	// Using std::array for safer command construction
	std::array<const char*, 3> cmd = { { "cycle", "pause", nullptr } };
	mpv_command_async(m_mpv, 0, cmd.data());
}

void VideoPlayer::shutdown() {
	if (m_mpv_gl) {
		mpv_render_context_free(m_mpv_gl);
		m_mpv_gl = nullptr;
	}

	if (m_mpv) {
		mpv_destroy(m_mpv);
		m_mpv = nullptr;
	}
}

void VideoPlayer::set_render_update_callback(std::function<void()> callback) {
	m_render_update_callback = std::move(callback);
}

void VideoPlayer::set_event_callback(std::function<void()> callback) {
	m_event_callback = std::move(callback);
}

void VideoPlayer::on_mpv_render_update(void* ctx) {
	auto* player = static_cast<VideoPlayer*>(ctx);
	if (player && player->m_render_update_callback) {
		player->m_render_update_callback();
	}
}

void VideoPlayer::on_mpv_events(void* ctx) {
	auto* player = static_cast<VideoPlayer*>(ctx);
	if (player && player->m_event_callback) {
		player->m_event_callback();
	}
}
