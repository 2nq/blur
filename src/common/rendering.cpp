#include "rendering.h"
#include "config_presets.h"
#include "config_blur.h"
#include "config_app.h"

tl::expected<nlohmann::json, std::string> rendering::detail::merge_settings(
	const BlurSettings& blur_settings, const GlobalAppSettings& app_settings
) {
	auto settings_json = blur_settings.to_json();
	if (!settings_json)
		return settings_json;

	auto app_json = app_settings.to_json();
	if (!app_json)
		return tl::unexpected(app_json.error());

	settings_json->update(*app_json);
	return settings_json;
}

std::vector<std::string> rendering::detail::build_vspipe_args(
	const std::filesystem::path& input_path, const nlohmann::json& merged_settings, bool is_video
) {
	std::string path_str = u::path_to_string(input_path);
	std::ranges::replace(path_str, '\\', '/');

	std::vector<std::string> args = {
		"-p",
		"-o",
		is_video ? "0" : "1",
		"-c",
		is_video ? "y4m" : "wav",
		"-a",
		"video_path=" + path_str,
		"-a",
		"settings=" + merged_settings.dump(),
	};

#ifdef __APPLE__
	args.insert(args.end(), { "-a", std::format("macos_bundled={}", blur.used_installer ? "true" : "false") });
#endif
#ifdef _WIN32
	args.insert(args.end(), { "-a", "enable_lsmash=true" });
#endif
#ifdef __linux__
	bool bundled = std::filesystem::exists(blur.resources_path / "vapoursynth-plugins");
	args.insert(args.end(), { "-a", std::format("linux_bundled={}", bundled ? "true" : "false") });
#endif

	args.insert(args.end(), { u::path_to_string(blur.resources_path / "lib/blur.py"), "-" });
	return args;
}

tl::expected<std::filesystem::path, std::string> rendering::detail::create_temp_output_path(
	const std::string& prefix, const std::string& extension
) {
	static std::atomic<size_t> counter = 0;
	auto temp_path = blur.create_temp_path(std::format("{}-{}", prefix, counter++));

	if (!temp_path)
		return tl::unexpected("Failed to create temp path");

	return *temp_path / std::format("output.{}", extension);
}

tl::expected<std::filesystem::path, std::string> rendering::detail::build_output_filename(
	const std::filesystem::path& input_path, const BlurSettings& settings, const GlobalAppSettings& app_settings
) {
	auto output_folder = input_path.parent_path() / app_settings.output_prefix;

	try {
		std::filesystem::create_directories(output_folder);
	}
	catch (const std::filesystem::filesystem_error& e) {
		return tl::unexpected(fmt::format("Failed to create output directory: {}", e.what()));
	}

	std::string base_name = std::format("{} - blur", input_path.stem());

	if (settings.detailed_filenames) {
		std::string details;
		if (settings.blur && settings.interpolate) {
			details = std::format(
				"{}fps ({}, {})", settings.blur_output_fps, settings.interpolated_fps, settings.blur_amount
			);
		}
		else if (settings.blur) {
			details = std::format("{}fps ({})", settings.blur_output_fps, settings.blur_amount);
		}
		else if (settings.interpolate) {
			details = std::format("{}fps", settings.interpolated_fps);
		}

		if (!details.empty())
			base_name += " ~ " + details;
	}

	// find unique filename
	int counter = 1;
	std::filesystem::path result;
	do {
		std::string filename = base_name;
		if (counter > 1)
			filename += std::format(" ({})", counter);
		filename += "." + settings.advanced.video_container;
		result = output_folder / filename;
		counter++;
	}
	while (std::filesystem::exists(result));

	return result;
}

std::vector<std::string> rendering::detail::build_color_metadata_args(const u::VideoInfo& video_info) {
	std::vector<std::string> params;

	if (video_info.color_range) {
		std::string range = *video_info.color_range == "pc" ? "full" : "limited";
		params.emplace_back("range=" + range);
	}

	if (video_info.color_space)
		params.emplace_back("colorspace=" + *video_info.color_space);

	if (video_info.color_transfer)
		params.emplace_back("color_trc=" + *video_info.color_transfer);

	if (video_info.color_primaries)
		params.emplace_back("color_primaries=" + *video_info.color_primaries);

	if (params.empty())
		return {};

	std::string filter =
		"setparams=" +
		std::accumulate(
			std::next(params.begin()), params.end(), params[0], [](const std::string& a, const std::string& b) {
				return a + ":" + b;
			}
		);

	std::vector<std::string> result = { "-vf", filter };

	if (video_info.pix_fmt) {
		result.insert(result.end(), { "-pix_fmt", *video_info.pix_fmt });
	}

	return result;
}

std::vector<std::string> rendering::detail::build_audio_filter_args(
	const BlurSettings& settings, const u::VideoInfo& video_info
) {
	if (!settings.timescale)
		return {};

	std::vector<std::string> filters;
	int sample_rate = video_info.sample_rate != -1 ? video_info.sample_rate : 48000;

	if (settings.input_timescale != 1.0f) {
		filters.push_back(std::format("asetrate={}*{}", sample_rate, 1.0f / settings.input_timescale));
		filters.push_back("aresample=48000");
	}

	if (settings.output_timescale != 1.0f) {
		if (settings.output_timescale_audio_pitch) {
			filters.push_back(std::format("asetrate={}*{}", sample_rate, settings.output_timescale));
			filters.push_back("aresample=48000");
		}
		else {
			filters.push_back(std::format("atempo={}", settings.output_timescale));
		}
	}

	if (filters.empty())
		return {};

	std::string combined = std::accumulate(
		std::next(filters.begin()), filters.end(), filters[0], [](const std::string& a, const std::string& b) {
			return a + "," + b;
		}
	);

	return { "-af", combined };
}

std::vector<std::string> rendering::detail::build_encoding_args(
	const BlurSettings& settings, const GlobalAppSettings& app_settings
) {
	if (!settings.advanced.ffmpeg_override.empty()) {
		auto args = u::ffmpeg_string_to_args(settings.advanced.ffmpeg_override);
		std::vector<std::string> result;
		result.reserve(args.size());
		for (const auto& arg : args) {
			result.push_back(arg);
		}
		return result;
	}

	auto preset_args = config_presets::get_preset_params(
		settings.gpu_encoding ? app_settings.gpu_type : "cpu",
		u::to_lower(settings.encode_preset.empty() ? "h264" : settings.encode_preset),
		settings.quality
	);

	std::vector<std::string> result;
	result.reserve(preset_args.size());
	for (const auto& arg : preset_args) {
		result.push_back(arg);
	}

	result.insert(result.end(), { "-c:a", "aac", "-b:a", "320k", "-movflags", "+faststart" });
	return result;
}

void rendering::detail::pause(int pid, const std::shared_ptr<RenderState>& state) {
	if (state->m_paused)
		return;

	if (pid > 0) {
#ifdef WIN32
		u::windows_toggle_suspend_process(pid, true);
#else
		kill(pid, SIGSTOP);
#endif
	}
	{
		std::lock_guard lock(state->m_mutex);
		state->m_paused = true;
	}

	state->m_progress.fps_initialised = false;
	state->m_progress.fps = 0.f;

	u::log("Render paused");
}

void rendering::detail::resume(int pid, const std::shared_ptr<RenderState>& state) {
	if (!state->m_paused)
		return;

	if (pid > 0) {
#ifdef WIN32
		u::windows_toggle_suspend_process(pid, false);
#else
		kill(pid, SIGCONT);
#endif
	}

	{
		std::lock_guard lock(state->m_mutex);
		state->m_paused = false;
	}

	u::log("Render resumed");
}

tl::expected<rendering::detail::PipelineResult, std::string> rendering::detail::execute_pipeline(
	RenderCommands commands,
	const std::shared_ptr<RenderState>& state,
	bool debug,
	bool audio,
	const std::function<void()>& progress_callback
) {
	namespace bp = boost::process;

	try {
		auto env = u::setup_vspipe_environment();

		// create temporary named pipes
		auto temp_dir = std::filesystem::temp_directory_path();
		auto video_pipe =
			temp_dir / ("blur_video_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

		std::filesystem::path audio_pipe;
		if (audio) {
			audio_pipe = temp_dir /
			             ("blur_audio_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		}

#ifdef _WIN32
		std::string video_pipe_name = R"(\\.\pipe\blur_video_)" + std::to_string(GetCurrentProcessId()) + "_" +
		                              std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
		std::string audio_pipe_name;
		if (audio) {
			audio_pipe_name = R"(\\.\pipe\blur_audio_)" + std::to_string(GetCurrentProcessId()) + "_" +
			                  std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
		}
#else
		// Unix FIFOs
		std::string video_pipe_name = video_pipe.string();
		std::string audio_pipe_name;

		if (mkfifo(video_pipe_name.c_str(), 0666) != 0) {
			return tl::unexpected("Failed to create video pipe: " + std::string(strerror(errno)));
		}

		if (audio) {
			audio_pipe_name = audio_pipe.string();
			if (mkfifo(audio_pipe_name.c_str(), 0666) != 0) {
				unlink(video_pipe_name.c_str());
				return tl::unexpected("Failed to create audio pipe: " + std::string(strerror(errno)));
			}
		}
#endif

		bp::ipstream vspipe_stderr;
		bp::ipstream vspipe_audio_stderr;
		bp::ipstream ffmpeg_stderr;

		// replace vspipe video placeholder
		for (auto& arg : commands.vspipe_video) {
			if (arg == "-") {
				arg = video_pipe_name;
			}
		}

		// replace vspipe audio placeholder
		if (audio) {
			for (auto& arg : commands.vspipe_audio) {
				if (arg == "-") {
					arg = audio_pipe_name;
				}
			}
		}

		// replace ffmpeg placeholders
		for (auto& arg : commands.ffmpeg) {
			if (audio && arg == "{audio_pipe}") {
				arg = audio_pipe_name;
			}
			else if (arg == "{video_pipe}") {
				arg = video_pipe_name;
			}
		}

		if (debug) {
			DEBUG_LOG("VSPipe video: {} {}", blur.vspipe_path, u::join(commands.vspipe_video, " "));
			if (audio) {
				DEBUG_LOG("VSPipe audio: {} {}", blur.vspipe_path, u::join(commands.vspipe_audio, " "));
			}
			DEBUG_LOG("FFmpeg: {} {}", blur.ffmpeg_path, u::join(commands.ffmpeg, " "));
		}

#ifndef _WIN32
		// Start FFmpeg process reading from named pipes
		auto ffmpeg_process = u::run_command(blur.ffmpeg_path, commands.ffmpeg, env, bp::std_err > ffmpeg_stderr);
#endif

		// Start VSPipe processes writing to named pipes
		auto vspipe_process = u::run_command(blur.vspipe_path, commands.vspipe_video, env, bp::std_err > vspipe_stderr);

		bp::child vspipe_audio_process;
		if (audio) {
#ifdef _WIN32
			vspipe_audio_process =
				u::run_command(blur.vspipe_path, commands.vspipe_audio, env, bp::std_err > vspipe_audio_stderr);
#else
			vspipe_audio_process =
				u::run_command(blur.vspipe_path, commands.vspipe_audio, env, bp::std_err > vspipe_audio_stderr);
#endif
		}

		std::ostringstream vspipe_errors;
		std::ostringstream vspipe_audio_errors;
		std::ostringstream ffmpeg_errors;

#ifdef _WIN32
		bool vspipe_ready = false;
		bool vspipe_audio_ready = false;
#endif

		std::thread vspipe_stderr_thread([&]() {
			std::string line;
			char ch = 0;
			while (vspipe_stderr.get(ch)) {
				if (ch == '\n') {
					vspipe_errors << line << '\n';

					DEBUG_LOG("[vspipe error] {}", line);

#ifdef _WIN32
					if (line == "Waiting for client to connect to named pipe...")
						vspipe_ready = true;
#endif

					line.clear();
				}
				else if (ch == '\r') {
					static std::regex frame_regex(R"(Frame: (\d+)\/(\d+)(?: \((\d+\.\d+) fps\))?)");

					std::smatch match;
					if (std::regex_match(line, match, frame_regex)) {
						{
							std::lock_guard lock(state->m_mutex);

							state->m_progress.current_frame = std::stoi(match[1]);
							state->m_progress.total_frames = std::stoi(match[2]);
							state->m_progress.rendered_a_frame = true;

							float progress = state->m_progress.current_frame / (float)state->m_progress.total_frames;

							if (!state->m_progress.fps_initialised) {
								state->m_progress.fps_initialised = true;
								state->m_progress.start_time = std::chrono::steady_clock::now();
								state->m_progress.start_frame = state->m_progress.current_frame;
								state->m_progress.fps = 0.f;

								state->m_progress.string = std::format(
									"{:.1f}% complete ({}/{})",
									progress * 100,
									state->m_progress.current_frame,
									state->m_progress.total_frames
								);
							}
							else {
								auto current_time = std::chrono::steady_clock::now();
								state->m_progress.elapsed_time = current_time - state->m_progress.start_time;

								state->m_progress.fps =
									(state->m_progress.current_frame - state->m_progress.start_frame) /
									state->m_progress.elapsed_time.count();

								state->m_progress.string = std::format(
									"{:.1f}% complete ({}/{}, {:.2f} fps)",
									progress * 100,
									state->m_progress.current_frame,
									state->m_progress.total_frames,
									state->m_progress.fps
								);
							}

							u::log(state->m_progress.string);
						}

						if (progress_callback)
							progress_callback();
					}

					line.clear();
				}
				else {
					line += ch;
				}
			}

			// process any remaining data in the pipe
			std::string remaining;
			while (std::getline(vspipe_stderr, remaining)) {
				vspipe_errors << remaining << '\n';
			}
		});

		std::unique_ptr<std::thread> vspipe_audio_stderr_thread;
		if (audio) {
			vspipe_audio_stderr_thread = std::make_unique<std::thread>([&]() {
				std::string line;
				while (std::getline(vspipe_audio_stderr, line)) {
					vspipe_audio_errors << line << '\n';

#ifdef _WIN32
					if (line == "Waiting for client to connect to named pipe...")
						vspipe_audio_ready = true;
#endif

					DEBUG_LOG("[vspipe audio error] {}", line);
				}
			});
		}

		std::thread ffmpeg_stderr_thread([&]() {
			std::string line;
			while (std::getline(ffmpeg_stderr, line)) {
				ffmpeg_errors << line << '\n';

				DEBUG_LOG("[ffmpeg error] {}", line);
			}
		});

#ifdef _WIN32
		while (!vspipe_ready || !vspipe_audio_ready) {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}

		// pipes are waiting for clients, time for ffmpeg to connect
		auto ffmpeg_process = u::run_command(blur.ffmpeg_path, commands.ffmpeg, env, bp::std_err > ffmpeg_stderr);
#endif

		bool killed = false;
		while (ffmpeg_process.running()) {
			if (state->m_to_stop) {
				vspipe_process.terminate();
				if (audio && vspipe_audio_process) {
					vspipe_audio_process.terminate();
				}
				ffmpeg_process.terminate();
				killed = true;
				break;
			}

			if (state->m_to_pause != state->m_paused) {
				auto fn = state->m_to_pause ? pause : resume;
				fn(vspipe_process.id(), state);
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}

		// stop stuff if they're stuck
		vspipe_process.terminate();
		if (audio && vspipe_audio_process) {
			vspipe_audio_process.terminate();
		}

		// wait for threads to finish
		if (ffmpeg_stderr_thread.joinable())
			ffmpeg_stderr_thread.join();

		if (vspipe_stderr_thread.joinable())
			vspipe_stderr_thread.join();

		if (audio && vspipe_audio_stderr_thread && vspipe_audio_stderr_thread->joinable())
			vspipe_audio_stderr_thread->join();

		// Clean up named pipes
#ifndef _WIN32
		unlink(video_pipe_name.c_str());
		if (audio && !audio_pipe_name.empty()) {
			unlink(audio_pipe_name.c_str());
		}
#endif

		if (killed)
			return PipelineResult{ .stopped = true };

		bool vspipe_video_failed = vspipe_process.exit_code() != 0;
		bool vspipe_audio_failed = audio && vspipe_audio_process.exit_code() != 0;
		bool ffmpeg_failed = ffmpeg_process.exit_code() != 0;

		if ((!commands.vspipe_will_stop_early && (vspipe_video_failed || vspipe_audio_failed)) || ffmpeg_failed) {
			std::string error_msg = "--- [vspipe] ---\n" + vspipe_errors.str();

			if (audio) {
				error_msg += "--- [vspipe audio] ---\n" + vspipe_audio_errors.str() + "\n";
			}

			error_msg += "--- [ffmpeg] ---\n" + ffmpeg_errors.str();

			return tl::unexpected(error_msg);
		}

		return PipelineResult{ .stopped = false };
	}
	catch (const std::exception& e) {
		return tl::unexpected(std::string(e.what()));
	}
}

void rendering::detail::copy_file_timestamp(const std::filesystem::path& from, const std::filesystem::path& to) {
	try {
		auto timestamp = std::filesystem::last_write_time(from);
		std::filesystem::last_write_time(to, timestamp);
	}
	catch (const std::exception& e) {
		u::log_error("Failed to copy timestamp: {}", e.what());
	}
}

tl::expected<rendering::RenderResult, std::string> rendering::render_frame(
	const std::filesystem::path& input_path,
	const BlurSettings& settings,
	const GlobalAppSettings& app_settings,
	const std::shared_ptr<RenderState>& state
) {
	if (!blur.initialised)
		return tl::unexpected("Blur not initialised");

	if (!std::filesystem::exists(input_path))
		return tl::unexpected("Input path does not exist");

	auto merged_settings = detail::merge_settings(settings, app_settings);
	if (!merged_settings)
		return tl::unexpected(merged_settings.error());

	auto output_path = detail::create_temp_output_path("frame-preview");
	if (!output_path)
		return tl::unexpected(output_path.error());

	RenderCommands commands = { .vspipe_video = detail::build_vspipe_args(input_path, *merged_settings, true),.vspipe_audio = detail::build_vspipe_args(input_path, *merged_settings, false),
		                        .ffmpeg = { "-loglevel",
		                                    "error",
		                                    "-hide_banner",
		                                    "-stats",
		                                    "-ss",
		                                    "00:00:00.200",
		                                    "-y",
		                                    "-i",
		                                    "{video_pipe}",
											"-map",
											"0:v",
		                                    "-vframes",
		                                    "1",
		                                    "-q:v",
		                                    "2",
		                                    "-y",
		                                    output_path->string(), }, .vspipe_will_stop_early = true, };

	auto pipeline_result = detail::execute_pipeline(commands, state, settings.advanced.debug, false, nullptr);
	if (!pipeline_result)
		return tl::unexpected(pipeline_result.error());

	return RenderResult{ .output_path = *output_path, .stopped = pipeline_result->stopped };
}

tl::expected<rendering::RenderResult, std::string> rendering::detail::render_video(
	const std::filesystem::path& input_path,
	const u::VideoInfo& video_info,
	const BlurSettings& settings,
	const std::shared_ptr<RenderState>& state,
	const GlobalAppSettings& app_settings,
	const std::optional<std::filesystem::path>& output_path_override,
	float start,
	float end,
	const std::function<void()>& progress_callback
) {
	if (!blur.initialised)
		return tl::unexpected("Blur not initialised");
	if (!std::filesystem::exists(input_path))
		return tl::unexpected("Input path does not exist");

	auto merged_settings = detail::merge_settings(settings, app_settings);
	if (!merged_settings)
		return tl::unexpected(merged_settings.error());

	std::filesystem::path output_path;
	if (output_path_override) {
		output_path = *output_path_override;
	}
	else {
		auto output_res = detail::build_output_filename(input_path, settings, app_settings);
		if (!output_res) {
			return tl::unexpected(output_res.error());
		}

		output_path = *output_res;
	}

	u::log("Rendering '{}'", input_path.stem());

	if (blur.verbose) {
		u::log("Source video at {:.2f} timescale", settings.input_timescale);
		if (settings.interpolate) {
			u::log("Interpolated to {}fps with {:.2f} timescale", settings.interpolated_fps, settings.output_timescale);
		}
		if (settings.blur) {
			u::log(
				"Motion blurred to {}fps ({}%)", settings.blur_output_fps, static_cast<int>(settings.blur_amount * 100)
			);
		}
		u::log("Rendered at {:.2f} speed with crf {}", settings.output_timescale, settings.quality);
	}

	// build vspipe command with video info
	auto vspipe_args = detail::build_vspipe_args(input_path, *merged_settings, true);
	vspipe_args.insert(
		vspipe_args.end() - 2,
		{
			// insert before script path and "-"
			"-a",
			std::format("fps_num={}", video_info.fps_num),
			"-a",
			std::format("fps_den={}", video_info.fps_den),
			"-a",
			"color_range=" + (video_info.color_range ? *video_info.color_range : "undefined"),
			"-a",
			std::format("start={}", start),
			"-a",
			std::format("end={}", end),
			"-a",
			std::format("has_audio={}", video_info.has_audio_stream),
		}
	);

	std::vector<std::string> vspipe_audio_args;

	if (video_info.has_audio_stream) {
		vspipe_audio_args = detail::build_vspipe_args(input_path, *merged_settings, false);
		vspipe_audio_args.insert(
			vspipe_audio_args.end() - 2,
			{
				// insert before script path and "-"
				"-a",
				std::format("fps_num={}", video_info.fps_num),
				"-a",
				std::format("fps_den={}", video_info.fps_den),
				"-a",
				"color_range=" + (video_info.color_range ? *video_info.color_range : "undefined"),
				"-a",
				std::format("start={}", start),
				"-a",
				std::format("end={}", end),
			}
		);
	}

	// build ffmpeg command
	std::vector<std::string> ffmpeg_args = {
		"-loglevel", "error", "-hide_banner", "-stats", "-y", "-fflags", "+genpts", "-i", "{video_pipe}",
	};

	if (video_info.has_audio_stream) {
		ffmpeg_args.insert(
			ffmpeg_args.end(),
			{
				"-i",
				"{audio_pipe}",
				"-map",
				"0:v",
				"-map",
				"1:a",
			}
		);
	}
	else {
		ffmpeg_args.insert(
			ffmpeg_args.end(),
			{
				"-map",
				"0:v",
			}
		);
	}

	// add color metadata
	auto color_args = detail::build_color_metadata_args(video_info);
	ffmpeg_args.insert(ffmpeg_args.end(), color_args.begin(), color_args.end());

	// add audio filters
	auto audio_args = detail::build_audio_filter_args(settings, video_info);
	ffmpeg_args.insert(ffmpeg_args.end(), audio_args.begin(), audio_args.end());

	// add encoding settings
	auto encoding_args = detail::build_encoding_args(settings, app_settings);
	ffmpeg_args.insert(ffmpeg_args.end(), encoding_args.begin(), encoding_args.end());

	ffmpeg_args.push_back(u::path_to_string(output_path));

	// add preview output if needed
	std::filesystem::path preview_path;
	if (settings.preview && blur.using_preview) {
		auto temp_preview = detail::create_temp_output_path("preview");
		if (temp_preview) {
			{
				std::lock_guard lock(state->m_mutex);
				state->m_preview_path = temp_preview.value();
			}

			ffmpeg_args.insert(
				ffmpeg_args.end(),
				{
					"-map",
					"0:v",
					"-q:v",
					"2",
					"-update",
					"1",
					"-atomic_writing",
					"1",
					"-y",
					u::path_to_string(state->m_preview_path),
				}
			);
		}
	}

	RenderCommands commands = {
		.vspipe_video = vspipe_args,
		.vspipe_audio = vspipe_audio_args,
		.ffmpeg = ffmpeg_args,
		.vspipe_will_stop_early = false,
	};

	auto pipeline_result = detail::execute_pipeline(
		commands, state, settings.advanced.debug, video_info.has_audio_stream, progress_callback
	);
	if (!pipeline_result)
		return tl::unexpected(pipeline_result.error());

	if (pipeline_result->stopped) {
		std::filesystem::remove(output_path);
		u::log("Stopped render '{}'", input_path.stem());
	}
	else {
		if (settings.copy_dates) {
			detail::copy_file_timestamp(input_path, output_path);
		}
		if (blur.verbose) {
			u::log("Finished rendering '{}'", input_path.stem());
		}
	}

	// clean up preview temp path if created
	if (!preview_path.empty()) {
		Blur::remove_temp_path(preview_path.parent_path());
	}

	return RenderResult{ .output_path = output_path, .stopped = pipeline_result->stopped };
}

rendering::QueueAddRes rendering::VideoRenderQueue::add(
	const std::filesystem::path& input_path,
	const u::VideoInfo& video_info,
	const std::optional<std::filesystem::path>& config_path,
	const GlobalAppSettings& app_settings,
	const std::optional<std::filesystem::path>& output_path_override,
	float start,
	float end,
	const std::function<void()>& progress_callback,
	const std::function<
		void(const VideoRenderDetails& render, const tl::expected<rendering::RenderResult, std::string>& result)>&
		finish_callback
) {
	// parse config file (do it now, not when rendering. nice for batch rendering the same file with different
	// settings)
	auto config_res = config_blur::get_config(
		config_path.has_value() ? config_path.value() : config_blur::get_config_filename(input_path.parent_path()),
		!config_path.has_value() // use global only if no config path is specified
	);

	// check if preset is valid
	auto valid_presets = u::get_supported_presets(config_res.config.gpu_encoding, app_settings.gpu_type);
	if (!u::contains(valid_presets, config_res.config.encode_preset)) {
		return {
			.is_global_config = config_res.is_global,
			.error = std::format("preset '{}' is not valid", config_res.config.encode_preset),
		};
	}

	std::lock_guard lock(m_mutex);
	auto added = m_queue.emplace_back(
		VideoRenderDetails{
			.input_path = input_path,
			.video_info = video_info,
			.settings = config_res.config,
			.app_settings = app_settings,
			.output_path_override = output_path_override,
			.start = start,
			.end = end,
			.progress_callback = progress_callback,
			.finish_callback = finish_callback,
		}
	);

	return {
		.is_global_config = config_res.is_global,
		.state = added.state,
	};
}
