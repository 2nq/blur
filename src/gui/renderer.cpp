#include "renderer.h"

#include "common/config_app.h"
#include "common/rendering.h"
#include "common/rendering_frame.h"

#include "gui/ui/keys.h"
#include "sdl.h"
#include "tasks.h"
#include "gui.h"

#include "ui/ui.h"
#include "render/render.h"
#include "os/desktop_notification.h"

#define DEBUG_RENDER 0

const int PAD_X = 24;
const int PAD_Y = PAD_X;

const int NOTIFICATIONS_PAD_X = 10;
const int NOTIFICATIONS_PAD_Y = 10;

const float FPS_SMOOTHING = 0.95f;

const uint8_t MUTED_SHADE = 155;

void gui::renderer::components::render(
	ui::Container& container,
	Render& render,
	bool current,
	float delta_time,
	bool& is_progress_shown,
	float& bar_percent
) {
	// todo: ui concept
	// screen start|      [faded]last_video current_video [faded]next_video next_video2 next_video3 (+5) |
	// screen end animate sliding in as it moves along the queue
	ui::add_text(
		std::format("video {} name text", render.get_render_id()),
		container,
		u::tostring(render.get_video_name()),
		gfx::Color(255, 255, 255, (current ? 255 : 100)),
		fonts::smaller_header_font,
		FONT_CENTERED_X
	);

	if (!current)
		return;

	auto render_status = render.get_status();
	int bar_width = 300;

	std::string preview_path = render.get_preview_path().string();
	if (!preview_path.empty() && render_status.current_frame > 0) {
		auto element = ui::add_image(
			"preview image",
			container,
			preview_path,
			gfx::Size(container.get_usable_rect().w, container.get_usable_rect().h / 2),
			std::to_string(render_status.current_frame)
		);
		if (element) {
			bar_width = (*element)->rect.w;
		}
	}

	if (render_status.init) {
		float render_progress = (float)render_status.current_frame / (float)render_status.total_frames;
		bar_percent = u::lerp(bar_percent, render_progress, 5.f * delta_time, 0.005f);

		ui::add_bar(
			"progress bar",
			container,
			bar_percent,
			gfx::Color(51, 51, 51, 255),
			gfx::Color::white(),
			bar_width,
			std::format("{:.1f}%", render_progress * 100),
			gfx::Color::white(),
			&fonts::dejavu
		);

		container.push_element_gap(6);

		if (render.is_paused()) {
			ui::add_text(
				"paused text", container, "Paused", gfx::Color::white(MUTED_SHADE), fonts::dejavu, FONT_CENTERED_X
			);
		}

		ui::add_text(
			"progress text",
			container,
			std::format("frame {}/{}", render_status.current_frame, render_status.total_frames),
			gfx::Color::white(MUTED_SHADE),
			fonts::dejavu,
			FONT_CENTERED_X
		);

		container.pop_element_gap();

		ui::add_text(
			"progress text 2",
			container,
			std::format("{:.2f} frames per second", render_status.fps),
			gfx::Color::white(MUTED_SHADE),
			fonts::dejavu,
			FONT_CENTERED_X
		);

		is_progress_shown = true;
	}
	else {
		if (render.is_paused()) {
			ui::add_text(
				"paused text", container, "Paused", gfx::Color::white(MUTED_SHADE), fonts::dejavu, FONT_CENTERED_X
			);
		}
		else {
			ui::add_text(
				"initialising render text",
				container,
				"Initialising render...",
				gfx::Color::white(),
				fonts::dejavu,
				FONT_CENTERED_X
			);
		}
	}
}

void add_open_files_button(ui::Container& container, const std::string& label) {
	ui::add_button("open file button", container, label, fonts::dejavu, [] {
		static auto file_callback = [](void* userdata, const char* const* files, int filter) {
			if (files && *files) {
				std::vector<std::wstring> wpaths;

				std::span<const char* const> span_files(files, SIZE_MAX); // big size, we stop manually

				for (const auto& file : span_files) {
					if (file == nullptr)
						break; // null-terminated array

					wpaths.push_back(u::towstring(file));
				}

				tasks::add_files(wpaths);
			}
		};

		SDL_ShowOpenFileDialog(
			file_callback, // Properly typed callback function
			nullptr,       // userdata
			nullptr,       // parent window (nullptr for default)
			nullptr,       // file filters
			0,             // number of filters
			"",            // default path
			true           // allow multiple files
		);
	});
};

void gui::renderer::components::main_screen(ui::Container& container, float delta_time) {
	static float bar_percent = 0.f;

	bool queue_empty = rendering.get_queue().empty() && !current_render_copy;

	if (queue_empty) {
		bar_percent = 0.f;

		gfx::Point title_pos = container.get_usable_rect().center();
		if (container.rect.h > 275)
			title_pos.y = int(PAD_Y + fonts::header_font.height());
		else
			title_pos.y = 10 + fonts::header_font.height();

		ui::add_text_fixed(
			"blur title text", container, title_pos, "blur", gfx::Color::white(), fonts::header_font, FONT_CENTERED_X
		);

		if (initialisation_res && !initialisation_res->success) {
			ui::add_text(
				"failed to initialise text",
				main_container,
				"Failed to initialise",
				gfx::Color::white(),
				fonts::dejavu,
				FONT_CENTERED_X
			);

			ui::add_text(
				"failed to initialise reason",
				main_container,
				initialisation_res->error_message,
				gfx::Color::white(MUTED_SHADE),
				fonts::dejavu,
				FONT_CENTERED_X
			);

			return;
		}

		add_open_files_button(container, "Open files");

		ui::add_text(
			"drop file text", container, "or drop them anywhere", gfx::Color::white(), fonts::dejavu, FONT_CENTERED_X
		);
	}
	else {
		bool is_progress_shown = false;

		rendering.lock();
		{
			auto current_render = rendering.get_current_render();

			// displays final state of the current render once where it would have been skipped otherwise
			auto render_current_edge_case = [&] {
				if (!current_render_copy)
					return;

				if (current_render) {
					if ((*current_render)->get_render_id() == (*current_render_copy).get_render_id()) {
						u::log("render final frame: wasnt deleted so just render normally");
						return;
					}
				}

				u::log("render final frame: it was deleted bro, rendering separately");

				components::render(container, *current_render_copy, true, delta_time, is_progress_shown, bar_percent);

				current_render_copy.reset();
			};

			render_current_edge_case();

			for (const auto [i, render] : u::enumerate(rendering.get_queue())) {
				bool current = current_render && render.get() == current_render.value();

				components::render(container, *render, current, delta_time, is_progress_shown, bar_percent);
			}
		}
		rendering.unlock();

		if (!is_progress_shown) {
			bar_percent = 0.f; // Reset when no progress bar is shown
		}
	}
}

void gui::renderer::components::configs::set_interpolated_fps() {
	if (interpolate_scale) {
		settings.interpolated_fps = std::format("{}x", interpolated_fps_mult);
	}
	else {
		settings.interpolated_fps = std::to_string(interpolated_fps);
	}

	if (pre_interpolate_scale) {
		if (interpolate_scale)
			pre_interpolated_fps_mult =
				std::min(pre_interpolated_fps_mult, interpolated_fps_mult); // can't preinterpolate more than interp

		settings.pre_interpolated_fps = std::format("{}x", pre_interpolated_fps_mult);
	}
	else {
		if (!interpolate_scale)
			pre_interpolated_fps =
				std::min(pre_interpolated_fps, interpolated_fps); // can't preinterpolate more than interp

		settings.pre_interpolated_fps = std::to_string(pre_interpolated_fps);
	}
}

void gui::renderer::components::configs::options(ui::Container& container, BlurSettings& settings) {
	static const gfx::Color section_color = gfx::Color::white(MUTED_SHADE);

	bool first_section = true;
	auto section_component = [&](std::string label, bool* setting = nullptr, bool forced_on = false) {
		if (!first_section) {
			ui::add_separator(std::format("section {} separator", label), container, ui::SeparatorStyle::FADE_RIGHT);
		}
		else
			first_section = false;

		if (!setting)
			return;

		if (!forced_on) {
			ui::add_checkbox(std::format("section {} checkbox", label), container, label, *setting, fonts::dejavu);
		}
		else {
			ui::add_text(std::format("section {}", label), container, label, gfx::Color::white(), fonts::dejavu);

			ui::add_text(
				std::format("section {} forced", label),
				container,
				"forced on as settings in this section have been modified",
				gfx::Color::white(MUTED_SHADE),
				fonts::dejavu
			);
		}
	};

	/*
	    Blur
	*/
	section_component("blur", &settings.blur);

	if (settings.blur) {
		ui::add_slider_tied(
			"blur amount",
			container,
			0.f,
			2.f,
			&settings.blur_amount,
			"blur amount: {:.2f}",
			&settings.blur_output_fps,
			settings.blur_amount_tied_to_fps,
			"fps",
			fonts::dejavu
		);

		ui::add_slider("output fps", container, 1, 120, &settings.blur_output_fps, "output fps: {} fps", fonts::dejavu);
		ui::add_dropdown(
			"blur weighting",
			container,
			"blur weighting",
			{
				"equal",
				"gaussian_sym",
				"vegas",
				"pyramid",
				"gaussian",
				"ascending",
				"descending",
				"gaussian_reverse",
			},
			settings.blur_weighting,
			fonts::dejavu
		);
		ui::add_slider("blur gamma", container, 1.f, 10.f, &settings.blur_gamma, "blur gamma: {:.2f}", fonts::dejavu);
	}

	/*
	    Interpolation
	*/
	section_component("interpolation", &settings.interpolate);

	if (settings.interpolate) {
		ui::add_checkbox(
			"interpolate scale checkbox",
			container,
			"interpolate by scaling fps",
			interpolate_scale,
			fonts::dejavu,
			[&](bool new_value) {
				set_interpolated_fps();
			}
		);

		if (interpolate_scale) {
			ui::add_slider(
				"interpolated fps mult",
				container,
				1.f,
				10.f,
				&interpolated_fps_mult,
				"interpolated fps: {:.1f}x",
				fonts::dejavu,
				[&](std::variant<int*, float*> value) {
					set_interpolated_fps();
				},
				0.1f
			);
		}
		else {
			ui::add_slider(
				"interpolated fps",
				container,
				1,
				2400,
				&interpolated_fps,
				"interpolated fps: {} fps",
				fonts::dejavu,
				[&](std::variant<int*, float*> value) {
					set_interpolated_fps();
				}
			);
		}

#ifndef __APPLE__ // see comment
		ui::add_dropdown(
			"interpolation method dropdown",
			container,
			"interpolation method",
			{
				"svp",
				"rife", // plugins broken on mac rn idk why todo: fix when its fixed
			},
			settings.interpolation_method,
			fonts::dejavu
		);
#endif
	}

#ifndef __APPLE__ // see above
	/*
	    Pre-interpolation
	*/
	if (settings.interpolate && settings.interpolation_method != "rife") {
		section_component("pre-interpolation", &settings.pre_interpolate);

		if (settings.pre_interpolate) {
			ui::add_checkbox(
				"pre-interpolate scale checkbox",
				container,
				"pre-interpolate by scaling fps",
				pre_interpolate_scale,
				fonts::dejavu,
				[&](bool new_value) {
					set_interpolated_fps();
				}
			);

			if (pre_interpolate_scale) {
				ui::add_slider(
					"pre-interpolated fps mult",
					container,
					1.f,
					interpolate_scale ? interpolated_fps_mult : 10.f,
					&pre_interpolated_fps_mult,
					"pre-interpolated fps: {:.1f}x",
					fonts::dejavu,
					[&](std::variant<int*, float*> value) {
						set_interpolated_fps();
					},
					0.1f
				);
			}
			else {
				ui::add_slider(
					"pre-interpolated fps",
					container,
					1,
					!interpolate_scale ? interpolated_fps : 2400,
					&pre_interpolated_fps,
					"pre-interpolated fps: {} fps",
					fonts::dejavu,
					[&](std::variant<int*, float*> value) {
						set_interpolated_fps();
					}
				);
			}
		}
	}
#endif

	/*
	    Deduplication
	*/
	section_component("deduplication");

	ui::add_checkbox("deduplicate checkbox", container, "deduplicate", settings.deduplicate, fonts::dejavu);

	if (settings.deduplicate) {
		ui::add_dropdown(
			"deduplicate method dropdown",
			container,
			"deduplicate method",
			{ "svp",
#ifndef __APPLE__ // rife issue again
		      "rife",
#endif
		      "old" },
			settings.deduplicate_method,
			fonts::dejavu
		);
	}

	/*
	    Rendering
	*/
	section_component("rendering");

	ui::add_dropdown(
		"codec dropdown",
		container,
		std::format("encode preset ({})", settings.gpu_encoding ? "gpu: " + settings.gpu_type : "cpu"),
		u::get_supported_presets(settings.gpu_encoding, settings.gpu_type),
		settings.encode_preset,
		fonts::dejavu
	);

	if (settings.advanced.ffmpeg_override.empty()) {
		int min_quality = 0;
		int max_quality = 51;
		std::string quality_label = "quality: {}";

		if (settings.encode_preset == "prores" && settings.gpu_type == "mac") {
			min_quality = 0; // proxy
			max_quality = 3; // hq
			quality_label = "quality: {} (0:proxy, 1:lt, 2:standard, 3:hq)";
		}
		else if (settings.encode_preset == "av1") {
			max_quality = 63;
		}

		settings.quality = std::clamp(settings.quality, min_quality, max_quality);

		ui::add_slider(
			"quality", container, min_quality, max_quality, &settings.quality, quality_label, fonts::dejavu, {}, 0.f
		);
	}
	else {
		ui::add_text(
			"ffmpeg override quality warning",
			container,
			"quality overridden by custom ffmpeg filters",
			gfx::Color(252, 186, 3, 150),
			fonts::dejavu
		);
	}

	ui::add_checkbox("preview checkbox", container, "preview", settings.preview, fonts::dejavu);

	ui::add_checkbox(
		"detailed filenames checkbox", container, "detailed filenames", settings.detailed_filenames, fonts::dejavu
	);

	ui::add_checkbox("copy dates checkbox", container, "copy dates", settings.copy_dates, fonts::dejavu);

	/*
	    GPU Acceleration
	*/
	section_component("gpu acceleration");

	ui::add_checkbox("gpu decoding checkbox", container, "gpu decoding", settings.gpu_decoding, fonts::dejavu);

	ui::add_checkbox(
		"gpu interpolation checkbox", container, "gpu interpolation", settings.gpu_interpolation, fonts::dejavu
	);

	if (settings.advanced.ffmpeg_override.empty()) {
		ui::add_checkbox("gpu encoding checkbox", container, "gpu encoding", settings.gpu_encoding, fonts::dejavu);

		if (settings.gpu_encoding) {
			auto gpu_types = u::get_available_gpu_types();
			if (gpu_types.size() > 1) {
				ui::add_dropdown(
					"gpu encoding type dropdown",
					container,
					"gpu encoding device",
					gpu_types,
					settings.gpu_type,
					fonts::dejavu
				);
			}
		}
	}
	else {
		ui::add_text(
			"ffmpeg override gpu encoding warning",
			container,
			"gpu encoding overridden by custom ffmpeg filters",
			gfx::Color(252, 186, 3, 150),
			fonts::dejavu
		);
	}

#ifndef __APPLE__ // rife mac issue todo:
	static std::string rife_gpu;

	if (settings.rife_gpu_index == -1) {
		rife_gpu = "default - will use first available";
	}
	else {
		if (blur.initialised_rife_gpus) {
			rife_gpu = blur.rife_gpus.at(settings.rife_gpu_index);
		}
		else {
			rife_gpu = std::format("gpu {}", settings.rife_gpu_index);
		}
	}

	ui::add_dropdown(
		"rife gpu dropdown",
		container,
		"rife gpu",
		blur.rife_gpu_names,
		rife_gpu,
		fonts::dejavu,
		[&](std::string* new_gpu_name) {
			for (const auto& [gpu_index, gpu_name] : blur.rife_gpus) {
				if (gpu_name == *new_gpu_name) {
					settings.rife_gpu_index = gpu_index;
				}
			}
		}
	);
#endif

	/*
	    Timescale
	*/
	section_component("timescale", &settings.timescale);

	if (settings.timescale) {
		ui::add_slider(
			"input timescale",
			container,
			0.f,
			2.f,
			&settings.input_timescale,
			"input timescale: {:.2f}",
			fonts::dejavu,
			{},
			0.01f
		);

		ui::add_slider(
			"output timescale",
			container,
			0.f,
			2.f,
			&settings.output_timescale,
			"output timescale: {:.2f}",
			fonts::dejavu,
			{},
			0.01f
		);

		ui::add_checkbox(
			"adjust timescaled audio pitch checkbox",
			container,
			"adjust timescaled audio pitch",
			settings.output_timescale_audio_pitch,
			fonts::dejavu
		);
	}

	/*
	    Filters
	*/
	section_component("filters", &settings.filters);

	if (settings.filters) {
		ui::add_slider(
			"brightness", container, 0.f, 2.f, &settings.brightness, "brightness: {:.2f}", fonts::dejavu, {}, 0.01f
		);
		ui::add_slider(
			"saturation", container, 0.f, 2.f, &settings.saturation, "saturation: {:.2f}", fonts::dejavu, {}, 0.01f
		);
		ui::add_slider(
			"contrast", container, 0.f, 2.f, &settings.contrast, "contrast: {:.2f}", fonts::dejavu, {}, 0.01f
		);
	}

	bool modified_advanced = settings.advanced != config_blur::DEFAULT_CONFIG.advanced;

	section_component("advanced", &settings.override_advanced, modified_advanced);

	if (settings.override_advanced) {
		/*
		    Advanced Deduplication
		*/
		section_component("advanced deduplication");

		if (settings.deduplicate_method != "old") {
			ui::add_slider(
				"deduplicate range",
				container,
				-1,
				10,
				&settings.advanced.deduplicate_range,
				"deduplicate range: {}",
				fonts::dejavu,
				{},
				0.f,
				"-1 = infinite"
			);
		}

		std::istringstream iss(settings.advanced.deduplicate_threshold);
		float f = NAN;
		iss >> std::noskipws >> f; // try to read as float
		bool is_float = iss.eof() && !iss.fail();

		if (!is_float)
			container.push_element_gap(2);

		ui::add_text_input(
			"deduplicate threshold input",
			container,
			settings.advanced.deduplicate_threshold,
			"deduplicate threshold",
			fonts::dejavu
		);

		if (!is_float) {
			container.pop_element_gap();

			ui::add_text(
				"deduplicate threshold not a float warning",
				container,
				"deduplicate threshold must be a decimal number",
				gfx::Color(255, 0, 0, 255),
				fonts::dejavu
			);
		}

		/*
		    Advanced Rendering
		*/
		section_component("advanced rendering");

		ui::add_text_input(
			"video container text input", container, settings.advanced.video_container, "video container", fonts::dejavu
		);

		bool bad_audio = settings.timescale && (u::contains(settings.advanced.ffmpeg_override, "-c:a copy") ||
		                                        u::contains(settings.advanced.ffmpeg_override, "-codec:a copy"));
		if (bad_audio)
			container.push_element_gap(2);

		ui::add_text_input(
			"custom ffmpeg filters text input",
			container,
			settings.advanced.ffmpeg_override,
			"custom ffmpeg filters",
			fonts::dejavu
		);

		if (bad_audio) {
			container.pop_element_gap();

			ui::add_text(
				"timescale audio copy warning",
				container,
				"cannot use -c:a copy while using timescale",
				gfx::Color(255, 0, 0, 255),
				fonts::dejavu
			);
		}

		ui::add_checkbox("debug checkbox", container, "debug", settings.advanced.debug, fonts::dejavu);

		/*
		    Advanced Interpolation
		*/
		section_component("advanced interpolation");

		if (settings.interpolation_method == "svp") {
			ui::add_dropdown(
				"SVP interpolation preset dropdown",
				container,
				"SVP interpolation preset",
				config_blur::SVP_INTERPOLATION_PRESETS,
				settings.advanced.svp_interpolation_preset,
				fonts::dejavu
			);

			ui::add_dropdown(
				"SVP interpolation algorithm dropdown",
				container,
				"SVP interpolation algorithm",
				config_blur::SVP_INTERPOLATION_ALGORITHMS,
				settings.advanced.svp_interpolation_algorithm,
				fonts::dejavu
			);
		}

		ui::add_dropdown(
			"interpolation block size dropdown",
			container,
			"interpolation block size",
			config_blur::INTERPOLATION_BLOCK_SIZES,
			settings.advanced.interpolation_blocksize,
			fonts::dejavu
		);

		ui::add_slider(
			"interpolation mask area slider",
			container,
			0,
			500,
			&settings.advanced.interpolation_mask_area,
			"interpolation mask area: {}",
			fonts::dejavu
		);

#ifndef __APPLE__ // rife issue again
		ui::add_text_input("rife model", container, settings.advanced.rife_model, "rife model", fonts::dejavu);
#endif

		/*
		    Advanced Blur
		*/
		section_component("advanced blur");

		ui::add_slider(
			"blur weighting gaussian std dev slider",
			container,
			0.f,
			10.f,
			&settings.advanced.blur_weighting_gaussian_std_dev,
			"blur weighting gaussian std dev: {:.2f}",
			fonts::dejavu
		);
		ui::add_slider(
			"blur weighting gaussian mean slider",
			container,
			0.f,
			2.f,
			&settings.advanced.blur_weighting_gaussian_mean,
			"blur weighting gaussian mean: {:.2f}",
			fonts::dejavu
		);
		ui::add_text_input(
			"blur weighting gaussian bound input",
			container,
			settings.advanced.blur_weighting_gaussian_bound,
			"blur weighting gaussian bound",
			fonts::dejavu
		);
	}
	else {
		// make sure theres no funny business (TODO: is this needed, are there edge cases?)
		settings.advanced = config_blur::DEFAULT_CONFIG.advanced;
	}
}

// NOLINTBEGIN(readability-function-cognitive-complexity) todo: refactor
void gui::renderer::components::configs::preview(ui::Container& container, BlurSettings& settings) {
	static BlurSettings previewed_settings;
	static bool first = true;

	static auto debounce_time = std::chrono::milliseconds(50);
	auto now = std::chrono::steady_clock::now();
	static auto last_render_time = now;

	static size_t preview_id = 0;
	static std::filesystem::path preview_path;
	static bool loading = false;
	static bool error = false;
	static std::mutex preview_mutex;

	auto sample_video_path = blur.settings_path / "sample_video.mp4";
	bool sample_video_exists = std::filesystem::exists(sample_video_path);

	auto render_preview = [&] {
		if (!sample_video_exists) {
			preview_path.clear();
			return;
		}

		if (first) {
			first = false;
		}
		else {
			if (settings == previewed_settings && !first && !just_added_sample_video)
				return;

			if (now - last_render_time < debounce_time)
				return;
		}

		u::log("generating config preview");

		previewed_settings = settings;
		just_added_sample_video = false;
		last_render_time = now;

		{
			std::lock_guard<std::mutex> lock(preview_mutex);
			loading = true;
		}

		std::thread([sample_video_path, settings] {
			FrameRender* render = nullptr;

			{
				std::lock_guard<std::mutex> lock(render_mutex);

				// stop ongoing renders early, a new render's coming bro
				for (auto& render : renders) {
					render->stop();
				}

				render = renders.emplace_back(std::make_unique<FrameRender>()).get();
			}

			auto res = render->render(sample_video_path, settings);

			if (render == renders.back().get())
			{ // todo: this should be correct right? any cases where this doesn't work?
				loading = false;
				error = !res.success;

				if (!error) {
					std::lock_guard<std::mutex> lock(preview_mutex);
					preview_id++;

					Blur::remove_temp_path(preview_path.parent_path());

					preview_path = res.output_path;

					u::log("config preview finished rendering");
				}
				else {
					if (res.error_message != "Input path does not exist") {
						add_notification(
							"Failed to generate config preview. Click to copy error message",
							ui::NotificationType::NOTIF_ERROR,
							[res](const std::string& id) {
								SDL_SetClipboardText(res.error_message.c_str());

								// std::lock_guard<std::mutex> lock(notification_mutex);
								for (auto& n : notifications) {
									if (n.id == id) {
										n.closing = true;
										break;
									}
								}

								add_notification(
									"Copied error message to clipboard",
									ui::NotificationType::INFO,
									{},
									std::chrono::duration<float>(2.f)
								);
							}
						);
					}
				}
			}

			render->set_can_delete();
		}).detach();
	};

	render_preview();

	// remove finished renders
	{
		std::lock_guard<std::mutex> lock(render_mutex);
		std::erase_if(renders, [](const auto& render) {
			return render->can_delete();
		});
	}

	try {
		if (!preview_path.empty() && std::filesystem::exists(preview_path) && !error) {
			container.push_element_gap(6);
			ui::add_text(
				"preview header",
				container,
				"Config preview",
				gfx::Color::white(MUTED_SHADE),
				fonts::dejavu,
				FONT_CENTERED_X
			);
			container.pop_element_gap();

			auto element = ui::add_image(
				"config preview image",
				container,
				preview_path,
				container.get_usable_rect().size(),
				std::to_string(preview_id),
				gfx::Color::white(loading ? 100 : 255)
			);
		}
		else {
			if (sample_video_exists) {
				if (loading) {
					ui::add_text(
						"loading config preview text",
						container,
						"Loading config preview...",
						gfx::Color::white(100),
						fonts::dejavu,
						FONT_CENTERED_X
					);
				}
				else {
					ui::add_text(
						"failed to generate preview text",
						container,
						"Failed to generate preview.",
						gfx::Color::white(100),
						fonts::dejavu,
						FONT_CENTERED_X
					);
				}
			}
			else {
				ui::add_text(
					"sample video does not exist text",
					container,
					"No preview video found.",
					gfx::Color::white(100),
					fonts::dejavu,
					FONT_CENTERED_X
				);

				ui::add_text(
					"sample video does not exist text 2",
					container,
					"Drop a video here to add one.",
					gfx::Color::white(100),
					fonts::dejavu,
					FONT_CENTERED_X
				);

				ui::add_button("open preview file button", container, "Or open file", fonts::dejavu, [] {
					static auto file_callback = [](void* userdata, const char* const* files, int filter) {
						if (files != nullptr && *files != nullptr) {
							const char* file = *files;
							tasks::add_sample_video(u::towstring(file));
						}
					};

					SDL_ShowOpenFileDialog(
						file_callback, // callback
						nullptr,       // userdata
						nullptr,       // parent window
						nullptr,       // file filters
						0,             // number of filters
						"",            // default path
						false          // allow multiple files
					);
				});
			}
		}
	}
	catch (std::filesystem::filesystem_error& e) {
		// i have no idea. std::filesystem::exists threw?
		u::log_error("std::filesystem::exists threw");
	}

	ui::add_separator("config preview separator", container, ui::SeparatorStyle::FADE_BOTH);

	auto validation_res = config_blur::validate(settings, false);
	if (!validation_res.success) {
		ui::add_text(
			"config validation error/s",
			container,
			validation_res.error,
			gfx::Color(255, 50, 50, 255),
			fonts::dejavu,
			FONT_CENTERED_X | FONT_OUTLINE
		);

		ui::add_button("fix config button", container, "Reset invalid config options to defaults", fonts::dejavu, [&] {
			config_blur::validate(settings, true);
		});
	}

	ui::add_button("open config folder", container, "Open config folder", fonts::dejavu, [] {
		// Convert path to a file:// URL for SDL_OpenURL
		std::string file_url = "file://" + blur.settings_path.string();
		if (!SDL_OpenURL(file_url.c_str())) {
			u::log_error("Failed to open config folder: {}", SDL_GetError());
		}
	});
}

void gui::renderer::components::configs::option_information(ui::Container& container, BlurSettings& settings) {
	const static std::unordered_map<std::string, std::vector<std::string>> option_explanations = {
		// Blur settings
		// { "section blur checkbox",
		//   {
		// 	  "Enable motion blur",
		//   }, },
		{
			"blur amount",
			{
				"Amount of motion blur",
				"(0 = no blur, 1 = fully blend all frames, >1 = blend extra frames (ghosting))",
			},
		},
		// { "output fps",
		//   {
		// 	  "FPS of the output video",
		//   }, },
		{
			"blur gamma",
			{
				"Amount that the video is darkened before blurring. Makes highlights stand out",
			},
		},
		{
			"blur weighting gaussian std dev slider",
			{
				"Standard deviation for Gaussian blur weighting",
			},
		},
		{
			"blur weighting triangle reverse checkbox",
			{
				"Reverses the direction of triangle weighting",
			},
		},
		{
			"blur weighting bound input",
			{
				"Weighting bounds to spread weights more",
			},
		},

		// Interpolation settings
		// { "section interpolation checkbox",
		//   {
		// 	  "Enable interpolation to a higher FPS before blurring",
		//   }, },
		{
			"interpolate scale checkbox",
			{
				"Use a multiplier for FPS interpolation rather than a set FPS",
			},
		},
		{
			"interpolated fps mult",
			{
				"Multiplier for FPS interpolation",
				"The input video will be interpolated to this FPS (before blurring)",
			},
		},
		{
			"interpolated fps",
			{
				"FPS to interpolate input video to (before blurring)",
			},
		},
		{
			"interpolation method dropdown",
			{
				"Quality: rife > svp",
				"Speed: svp > rife",
#ifdef __APPLE__
				"SVP is faster, but requires that SVP Manager be open or a red border will appear",
#endif
			},
		},
		// pre-interp settings
		{
			"section pre-interpolation checkbox",
			{
				"Enable pre-interpolation using a more accurate but slower AI model before main interpolation",
			},
		},
		{
			"pre-interpolated fps mult",
			{
				"Multiplier for FPS pre-interpolation",
				"The input video will be interpolated to this FPS (before main interpolation and blurring)",
			},
		},
		{
			"pre-interpolated fps",
			{
				"FPS to pre-interpolate input video to (before blurring)",
			},
		},
		{
			"SVP interpolation preset dropdown",
			{
				"Check the blur GitHub for more information",
			},
		},
		{
			"SVP interpolation algorithm dropdown",
			{
				"Check the blur GitHub for more information",
			},
		},
		{
			"interpolation block size dropdown",
			{
				"Block size for interpolation",
				"(higher = less accurate, faster; lower = more accurate, slower)",
			},
		},
		{
			"interpolation mask area slider",
			{
				"Mask amount for interpolation",
				"(higher reduces blur on static objects but can affect smoothness)",
			},
		},

		// Rendering settings
		{
			"quality",
			{
				"Quality setting for output video. Depends on the codec",
				"(0 = lossless quality, 51 = really bad)",
			},
		},
		{
			"deduplicate checkbox",
			{
				"Removes duplicate frames and replaces them with interpolated frames",
				"(fixes 'unsmooth' looking output caused by stuttering in recordings)",
			},
		},
		{
			"deduplicate range",
			{
				"Amount of frames beyond the current frame to look for unique frames when deduplicating",
				"Make it higher if your footage is at a lower FPS than it should be, e.g. choppy 120fps gameplay "
				"recorded at 240fps",
				"Lower it if your blurred footage starts blurring static elements such as menu screens",
			},
		},
		{
			"deduplicate threshold input",
			{
				"Threshold of movement that triggers deduplication",
				"Turn on debug in advanced and render a video to embed text showing the movement in each frame",
			},
		},
		{
			"deduplicate method dropdown",
			{
				"Quality: rife > svp",
				"Speed: old > svp > rife",
			},
		},
		{
			"preview checkbox",
			{
				"Shows preview while rendering",
			},
		},
		{
			"detailed filenames checkbox",
			{
				"Adds blur settings to generated filenames",
			},
		},

		// Timescale settings
		{
			"section timescale checkbox",
			{
				"Enable video timescale manipulation",
			},
		},
		{
			"input timescale",
			{
				"Timescale of the input video file",
			},
		},
		{
			"output timescale",
			{
				"Timescale of the output video file",
			},
		},
		{
			"adjust timescaled audio pitch checkbox",
			{
				"Pitch shift audio when speeding up or slowing down video",
			},
		},

		// Filters
		// { "section filters checkbox", { "Enable video filters", }, },
		// { "brightness", { "Adjusts brightness of the output video", }, },
		// { "saturation", { "Adjusts saturation of the output video", }, },
		// { "contrast", { "Adjusts contrast of the output video", }, },

		// Advanced rendering
		// { "gpu interpolation checkbox", { "Uses GPU for interpolation", }, },
		// { "gpu encoding checkbox", { "Uses GPU for rendering", }, },
		// { "gpu encoding type dropdown", { "Select GPU type", }, },
		{
			"video container text input",
			{
				"Output video container format",
			},
		},
		{
			"custom ffmpeg filters text input",
			{
				"Custom FFmpeg filters for rendering",
				"(overrides GPU & quality options)",
			},
		},
		// { "debug checkbox", { "Shows debug window and prints commands used by blur", } }
		{
			"copy dates checkbox",
			{
				"Copies over the modified date from the input",
			},
		},
	};

	std::string hovered = ui::get_hovered_id();

	if (hovered.empty())
		return;

	if (!option_explanations.contains(hovered))
		return;

	ui::add_text(
		"hovered option info",
		container,
		option_explanations.at(hovered),
		gfx::Color::white(),
		fonts::dejavu,
		FONT_CENTERED_X | FONT_OUTLINE
	);
}

// NOLINTEND(readability-function-cognitive-complexity)

void gui::renderer::components::configs::parse_interp() {
	auto parse_fps_setting = [&](const std::string& fps_setting,
	                             int& fps,
	                             float& fps_mult,
	                             bool& scale_mode,
	                             const std::function<void()>& set_function,
	                             const std::string& log_prefix = "") {
		try {
			auto split = u::split_string(fps_setting, "x");
			if (split.size() > 1) {
				fps_mult = std::stof(split[0]);
				scale_mode = true;
			}
			else {
				fps = std::stof(fps_setting);
				scale_mode = false;
			}

			u::log("loaded {}interp, scale: {} (fps: {}, mult: {})", log_prefix, scale_mode, fps, fps_mult);
		}
		catch (std::exception& e) {
			u::log("failed to parse {}interpolated fps, setting defaults cos user error", log_prefix);
			set_function();
		}
	};

	parse_fps_setting(
		settings.interpolated_fps, interpolated_fps, interpolated_fps_mult, interpolate_scale, set_interpolated_fps
	);

	parse_fps_setting(
		settings.pre_interpolated_fps,
		pre_interpolated_fps,
		pre_interpolated_fps_mult,
		pre_interpolate_scale,
		set_interpolated_fps,
		"pre-"
	);
};

void gui::renderer::components::configs::save_config() {
	config_blur::create(config_blur::get_global_config_path(), settings);
	current_global_settings = settings;
};

void gui::renderer::components::configs::on_load() {
	current_global_settings = settings;
	parse_interp();
};

void gui::renderer::components::configs::screen(
	ui::Container& config_container,
	ui::Container& preview_container,
	ui::Container& option_information_container,
	float delta_time
) {
	static bool loading_config = false;
	if (!loaded_config) {
		if (!loading_config) {
			loading_config = true;

			std::thread([] {
				settings = config_blur::parse_global_config();
				on_load();
				loading_config = false;
				loaded_config = true;
			}).detach();
		}

		ui::add_text(
			"config loading text",
			config_container,
			"Loading config...",
			gfx::Color::white(100),
			fonts::dejavu,
			FONT_CENTERED_X
		);

		ui::center_elements_in_container(config_container);
		return;
	}

	bool config_changed = settings != current_global_settings;

	if (config_changed) {
		ui::set_next_same_line(nav_container);
		ui::add_button("save button", nav_container, "Save", fonts::dejavu, [&] {
			save_config();
		});

		ui::set_next_same_line(nav_container);
		ui::add_button("reset changes button", nav_container, "Reset changes", fonts::dejavu, [&] {
			settings = current_global_settings;
			on_load();
		});
	}

	auto modified_default = config_blur::DEFAULT_CONFIG;
	modified_default.gpu_type = settings.gpu_type;
	modified_default.rife_gpu_index =
		settings.rife_gpu_index; // the default config has uninitialised rife gpu, use index from current cfg to prevent
	                             // restore default from always showing up
	if (settings != modified_default) {
		ui::set_next_same_line(nav_container);
		ui::add_button("restore defaults button", nav_container, "Restore defaults", fonts::dejavu, [] {
			settings = config_blur::DEFAULT_CONFIG; // ^ actually restore defaults tho
			parse_interp();
		});
	}

	options(config_container, settings);
	preview(preview_container, settings);
	option_information(option_information_container, settings);
}

// NOLINTBEGIN(readability-function-size,readability-function-cognitive-complexity)

bool gui::renderer::redraw_window(bool rendered_last, bool force_render) {
	keys::on_frame_start();
	ui::on_frame_start();
	sdl::on_frame_start();

	render::update_window_size(sdl::window);

	auto now = std::chrono::steady_clock::now();
	static auto last_frame_time = now;

#if DEBUG_RENDER
	float fps = -1.f;
#endif
	float delta_time = NAN;

	if (!rendered_last) {
		delta_time = sdl::DEFAULT_DELTA_TIME;
	}
	else {
		float time_since_last_frame =
			std::chrono::duration<float>(std::chrono::steady_clock::now() - last_frame_time).count();

#if DEBUG_RENDER
		fps = 1.f / time_since_last_frame;

// float current_fps = 1.f / time_since_last_frame;
// if (fps == -1.f)
// 	fps = current_fps;
// fps = (fps * FPS_SMOOTHING) + (current_fps * (1.0f - FPS_SMOOTHING));
#endif

		delta_time = std::min(time_since_last_frame, sdl::MIN_DELTA_TIME);
	}

	last_frame_time = now;

	const gfx::Rect rect(gfx::Point(0, 0), render::window_size);

	static float bg_overlay_shade = 0.f;
	float last_fill_shade = bg_overlay_shade;
	bg_overlay_shade = u::lerp(bg_overlay_shade, gui::dragging ? 30.f : 0.f, 25.f * delta_time);
	force_render |= bg_overlay_shade != last_fill_shade;

	gfx::Rect nav_container_rect = rect;
	nav_container_rect.h = 70;
	nav_container_rect.y = rect.y2() - nav_container_rect.h;

	ui::reset_container(nav_container, sdl::window, nav_container_rect, fonts::dejavu.height(), {});

	int nav_cutoff = rect.y2() - nav_container_rect.y;
	int bottom_pad = std::max(PAD_Y, nav_cutoff);

	const static int main_pad_x = std::min(100, render::window_size.w / 10); // bit of magic never hurt anyone
	ui::reset_container(
		main_container, sdl::window, rect, 13, ui::Padding{ PAD_Y, main_pad_x, bottom_pad, main_pad_x }
	);

	const int config_page_container_gap = PAD_X / 2;

	gfx::Rect config_container_rect = rect;

	if (components::configs::loaded_config) {
		config_container_rect.w = 200 + PAD_X * 2;
	}

	ui::reset_container(
		config_container, sdl::window, config_container_rect, 9, ui::Padding{ PAD_Y, PAD_X, bottom_pad, PAD_X }
	);

	gfx::Rect config_preview_container_rect = rect;
	config_preview_container_rect.x = config_container_rect.x2() + config_page_container_gap;
	config_preview_container_rect.w -= config_container_rect.w + config_page_container_gap;

	ui::reset_container(
		config_preview_container,
		sdl::window,
		config_preview_container_rect,
		fonts::dejavu.height(),
		ui::Padding{ PAD_Y, PAD_X, bottom_pad, PAD_X }
	);

	ui::reset_container(
		option_information_container,
		sdl::window,
		config_preview_container_rect,
		9,
		ui::Padding{ PAD_Y, PAD_X, bottom_pad, PAD_X }
	);

	gfx::Rect notification_container_rect = rect;
	notification_container_rect.w = 230;
	notification_container_rect.x = rect.x2() - notification_container_rect.w - NOTIFICATIONS_PAD_X;
	notification_container_rect.h = 300;
	notification_container_rect.y = NOTIFICATIONS_PAD_Y;

	ui::reset_container(notification_container, sdl::window, notification_container_rect, 6, {});

	switch (screen) {
		case Screens::MAIN: {
			components::configs::loaded_config = false;

			components::main_screen(main_container, delta_time);

			if (initialisation_res && initialisation_res->success) {
				auto current_render = rendering.get_current_render();
				if (current_render) {
					ui::add_button(
						(*current_render)->is_paused() ? "resume render button" : "pause render button",
						nav_container,
						(*current_render)->is_paused() ? "Resume" : "Pause",
						fonts::dejavu,
						[] {
							auto current_render = rendering.get_current_render();
							if ((*current_render)->is_paused())
								(*current_render)->resume();
							else
								(*current_render)->pause();
						}
					);

					ui::set_next_same_line(nav_container);
					ui::add_button("stop render button", nav_container, "Cancel", fonts::dejavu, [] {
						auto current_render = rendering.get_current_render();
						if (current_render)
							(*current_render)->stop();
					});

					ui::set_next_same_line(nav_container);
					add_open_files_button(nav_container, "Add files");
				}

				ui::set_next_same_line(nav_container);
				ui::add_button("config button", nav_container, "Config", fonts::dejavu, [] {
					screen = Screens::CONFIG;
				});
			}

			ui::center_elements_in_container(main_container);

			break;
		}
		case Screens::CONFIG: {
			ui::set_next_same_line(nav_container);
			ui::add_button("back button", nav_container, "Back", fonts::dejavu, [] {
				screen = Screens::MAIN;
			});

			components::configs::screen(
				config_container, config_preview_container, option_information_container, delta_time
			);

			ui::center_elements_in_container(config_preview_container);
			ui::center_elements_in_container(option_information_container, true, false);

			break;
		}
	}

	render_notifications();

	ui::center_elements_in_container(nav_container);

	bool want_to_render = false;
	want_to_render |= ui::update_container_frame(notification_container, delta_time);
	want_to_render |= ui::update_container_frame(nav_container, delta_time);

	want_to_render |= ui::update_container_frame(main_container, delta_time);
	want_to_render |= ui::update_container_frame(config_container, delta_time);
	want_to_render |= ui::update_container_frame(config_preview_container, delta_time);
	want_to_render |= ui::update_container_frame(option_information_container, delta_time);
	ui::on_update_frame_end();

	if (!want_to_render && !force_render)
		// note: DONT RENDER ANYTHING ABOVE HERE!!! todo: render queue?
		return false;

	render::imgui.begin(sdl::window);
	{
		// background
		render::rect_filled(rect, gfx::Color(0, 0, 0, 255));

#if DEBUG_RENDER
		{
			// debug
			static const int debug_box_size = 30;
			static float x = rect.x2() - debug_box_size;
			static float y = 100.f;
			static bool right = false;
			static bool down = true;
			x += 1.f * (right ? 1 : -1);
			y += 1.f * (down ? 1 : -1);

			render::rect_filled(gfx::Rect(x, y, debug_box_size, debug_box_size), gfx::Color(255, 0, 0, 50));

			if (right) {
				if (x + debug_box_size > rect.x2())
					right = false;
			}
			else {
				if (x < 0)
					right = true;
			}

			if (down) {
				if (y + debug_box_size > rect.y2())
					down = false;
			}
			else {
				if (y < 0)
					down = true;
			}
		}
#endif

		ui::render_container(main_container);
		ui::render_container(config_container);
		ui::render_container(config_preview_container);
		ui::render_container(option_information_container);
		ui::render_container(nav_container);
		ui::render_container(notification_container);

		// file drop overlay
		if ((int)bg_overlay_shade > 0)
			render::rect_filled(rect, gfx::Color::white(bg_overlay_shade));

#if DEBUG_RENDER
		if (fps != -1.f) {
			gfx::Point fps_pos(rect.x2() - PAD_X, rect.y + PAD_Y);
			render::text(
				fps_pos, gfx::Color(0, 255, 0, 255), std::format("{:.0f} fps", fps), fonts::dejavu, FONT_RIGHT_ALIGN
			);
		}
#endif
	}
	render::imgui.end(sdl::window);

	ui::on_frame_end();

	return want_to_render;
}

// NOLINTEND(readability-function-size,readability-function-cognitive-complexity)

gui::renderer::Notification& gui::renderer::add_notification(
	const std::string& id,
	const std::string& text,
	ui::NotificationType type,
	const std::optional<std::function<void(const std::string& id)>>& on_click,
	std::optional<std::chrono::duration<float>> duration,
	bool closable
) {
	std::lock_guard<std::mutex> lock(notification_mutex);

	Notification new_notification{
		.id = id,
		.end_time = duration ? std::optional(
								   std::chrono::steady_clock::now() +
								   std::chrono::duration_cast<std::chrono::steady_clock::duration>(*duration)
							   )
		                     : std::nullopt,
		.text = text,
		.type = type,
		.on_click_fn = on_click,
		.closable = closable,
	};

	for (auto& notification : notifications) {
		if (notification.id == id) {
			notification = new_notification;
			return notification;
		}
	}

	return notifications.emplace_back(new_notification);
}

gui::renderer::Notification& gui::renderer::add_notification(
	const std::string& text,
	ui::NotificationType type,
	const std::optional<std::function<void(const std::string& id)>>& on_click,
	std::optional<std::chrono::duration<float>> duration,
	bool closable
) {
	static uint32_t current_notification_id = 0;
	return add_notification(
		std::format("notification {}", current_notification_id++), text, type, on_click, duration, closable
	);
}

void gui::renderer::on_render_finished(Render* render, const RenderResult& result) {
	if (result.stopped) {
		add_notification(
			std::format("Render '{}' stopped", u::tostring(render->get_video_name())), ui::NotificationType::INFO
		);
	}
	else if (result.success) {
		auto output_path = render->get_output_video_path();

		add_notification(
			std::format("Render '{}' completed", u::tostring(render->get_video_name())),
			ui::NotificationType::SUCCESS,
			[output_path](const std::string& id) {
				// Convert path to a file:// URL for SDL_OpenURL
				std::string file_url = "file://" + output_path.string();
				if (!SDL_OpenURL(file_url.c_str())) {
					u::log_error("Failed to open output folder: {}", SDL_GetError());
				}
			}
		);

		auto app_config = config_app::get_app_config();
		if (app_config.render_success_notifications) {
			desktop_notification::show("Render Complete", "Video render completed successfully");
		}
	}
	else {
		add_notification(
			std::format("Render '{}' failed. Click to copy error message", u::tostring(render->get_video_name())),
			ui::NotificationType::NOTIF_ERROR,
			[result](const std::string& id) {
				SDL_SetClipboardText(result.error_message.c_str());

				// std::lock_guard<std::mutex> lock(notification_mutex);
				for (auto& n : notifications) {
					if (n.id == id) {
						n.closing = true;
						break;
					}
				}

				add_notification(
					"Copied error message to clipboard",
					ui::NotificationType::INFO,
					{},
					std::chrono::duration<float>(2.f)
				);
			},
			std::nullopt
		);

		auto app_config = config_app::get_app_config();
		if (app_config.render_failure_notifications) {
			desktop_notification::show("Render Failed", std::format("Render failed: {}", result.error_message));
		}
	}
}

void gui::renderer::render_notifications() {
	std::lock_guard<std::mutex> lock(notification_mutex);

	auto now = std::chrono::steady_clock::now();

	for (auto it = notifications.begin(); it != notifications.end();) {
		if ((it->end_time && now > *it->end_time) || it->closing) {
			it = notifications.erase(it);
			continue;
		}

		ui::add_notification(
			it->id,
			notification_container,
			it->text,
			it->type,
			fonts::dejavu,
			it->on_click_fn,
			it->closable ? std::optional<std::function<void(const std::string&)>>([](const std::string& id) {
				for (auto& n : notifications) {
					if (n.id == id) {
						n.closing = true;
						break;
					}
				}
			})
						 : std::nullopt
		);

		++it;
	}
}
