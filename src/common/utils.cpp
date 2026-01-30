#include "utils.h"
#include "common/config_presets.h"
#include "common/config_app.h"

namespace {
	bool init_hw = false;
	// std::set<std::string> hw_accels; // TODO: re-add?
	// std::set<std::string> hw_encoders;
}

// NOLINTBEGIN gpt ass code
std::wstring u::towstring(const std::string& str) {
	if (str.empty())
		return std::wstring();

#ifdef _WIN32
	// Windows-specific implementation
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
	std::wstring result(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size_needed);
	return result;
#else
	// POSIX systems (Linux, macOS, etc.)
	std::vector<wchar_t> buf(str.size() + 1);
	std::mbstowcs(&buf[0], str.c_str(), str.size() + 1);
	return std::wstring(&buf[0]);
#endif
}

std::string u::tostring(const std::wstring& wstr) {
	if (wstr.empty()) {
		return std::string();
	}

#ifdef _WIN32
	// Windows-specific implementation
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
	std::string result(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &result[0], size_needed, nullptr, nullptr);
	return result;
#else
	// POSIX systems (Linux, macOS, etc.)
	std::vector<char> buf((wstr.size() + 1) * MB_CUR_MAX);
	size_t converted = std::wcstombs(&buf[0], wstr.c_str(), buf.size());
	if (converted == static_cast<size_t>(-1)) {
		return std::string(); // Conversion failed
	}
	return std::string(&buf[0], converted);
#endif
}

// NOLINTEND

std::string u::trim(std::string_view str) {
	str.remove_prefix(std::min(str.find_first_not_of(" \t\r\v\n"), str.size()));
	str.remove_suffix(std::min(str.size() - str.find_last_not_of(" \t\r\v\n") - 1, str.size()));

	return std::string(str);
}

std::string u::random_string(int len) {
	std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

	std::random_device rd;
	std::mt19937 generator(rd());

	std::shuffle(str.begin(), str.end(), generator);

	return str.substr(0, len);
}

std::vector<std::string> u::split_string(std::string str, const std::string& delimiter) {
	std::vector<std::string> output;

	size_t pos = 0;
	while ((pos = str.find(delimiter)) != std::string::npos) {
		std::string token = str.substr(0, pos);
		output.push_back(token);
		str.erase(0, pos + delimiter.length());
	}

	output.push_back(str);

	return output;
}

std::string u::to_lower(const std::string& str) {
	std::string out = str;

	std::ranges::for_each(out, [](char& c) {
		c = std::tolower(c);
	});

	return out;
}

std::string u::truncate_with_ellipsis(const std::string& input, std::size_t max_length) {
	const std::string ellipsis = "...";
	if (input.length() > max_length) {
		if (max_length <= ellipsis.length()) {
			return ellipsis.substr(0, max_length); // handle very small max_length
		}
		return input.substr(0, max_length - ellipsis.length()) + ellipsis;
	}
	return input;
}

std::optional<std::filesystem::path> u::get_program_path(const std::string& program_name) {
	namespace bp = boost::process;
	namespace fs = boost::filesystem;

	fs::path program_path = bp::search_path(program_name);

	std::filesystem::path path(program_path.native());

	if (!std::filesystem::exists(path))
		return {};

	return path;
}

// NOLINTBEGIN gpt ass code
std::string u::get_executable_path() {
#if defined(_WIN32)
	char path[MAX_PATH];
	GetModuleFileNameA(NULL, path, MAX_PATH);
	return std::string(path);
#elif defined(__linux__)
	char path[PATH_MAX];
	ssize_t count = readlink("/proc/self/exe", path, PATH_MAX);
	return std::string(path, (count > 0) ? count : 0);
#elif defined(__APPLE__)
	uint32_t size = 0;
	_NSGetExecutablePath(nullptr, &size); // Get the required size
	std::vector<char> path(size);
	if (_NSGetExecutablePath(path.data(), &size) == 0) {
		return std::string(path.data());
	}
	return "";
#else
#	error "Unsupported platform"
#endif
}

// NOLINTEND

constexpr int64_t PERIOD = 1;
constexpr int64_t TOLERANCE = 1'020'000;
constexpr int64_t MAX_TICKS = PERIOD * 9'500;

void u::sleep(double seconds) {
#ifndef WIN32
	std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
#else // KILLLLL WINDOWS
	using namespace std;
	using namespace chrono;

	auto t = high_resolution_clock::now();
	auto target = t + nanoseconds(int64_t(seconds * 1e9));

	static HANDLE timer;
	if (!timer)
		timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);

	int64_t maxTicks = PERIOD * 9'500;
	for (;;) {
		int64_t remaining = (target - t).count();
		int64_t ticks = (remaining - TOLERANCE) / 100;
		if (ticks <= 0)
			break;
		if (ticks > maxTicks)
			ticks = maxTicks;

		LARGE_INTEGER due;
		due.QuadPart = -ticks;
		SetWaitableTimerEx(timer, &due, 0, NULL, NULL, NULL, 0);
		WaitForSingleObject(timer, INFINITE);
		t = high_resolution_clock::now();
	}

	// spin
	while (high_resolution_clock::now() < target)
		YieldProcessor();
#endif
}

std::filesystem::path u::get_resources_path() {
#ifdef __APPLE__
	// Resources path if part of a macos bundle
	CFBundleRef bundle = CFBundleGetMainBundle();
	if (bundle) {
		CFURLRef resources_url = CFBundleCopyResourcesDirectoryURL(bundle);
		if (resources_url) {
			std::array<char, PATH_MAX> path{};
			if (CFURLGetFileSystemRepresentation(
					resources_url, static_cast<Boolean>(true), (UInt8*)path.data(), path.size() // NOLINT
				))
			{
				CFRelease(resources_url);
				return path.data();
			}
			CFRelease(resources_url);
		}
	}
#endif

	// binary path otherwise
	return std::filesystem::path(u::get_executable_path()).parent_path();
}

std::filesystem::path u::get_settings_path() {
	std::filesystem::path settings_path;
	const std::string app_name = "blur";

#if defined(_WIN32) || defined(_WIN64)
	// Windows: %APPDATA%\blur
	const char* app_data = std::getenv("APPDATA");
	if (app_data) {
		settings_path = std::filesystem::path(app_data) / app_name;
	}
	else {
		// Fallback if APPDATA is not available
		const char* user_profile = std::getenv("USERPROFILE");
		if (user_profile) {
			settings_path = std::filesystem::path(user_profile) / "AppData" / "Roaming" / app_name;
		}
	}
#elif defined(__APPLE__)
	// macOS: ~/Library/Application Support/blur
	const char* home = std::getenv("HOME");
	if (home) {
		settings_path = std::filesystem::path(home) / "Library" / "Application Support" / app_name;
	}
#else
	// Linux/Unix: ~/.config/blur (XDG convention)
	const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME");
	if (xdg_config_home && xdg_config_home[0] != '\0') {
		settings_path = std::filesystem::path(xdg_config_home) / app_name;
	}
	else {
		const char* home = std::getenv("HOME");
		if (home) {
			settings_path = std::filesystem::path(home) / ".config" / app_name;
		}
	}
#endif

	// Create directories if they don't exist
	if (!settings_path.empty()) {
		std::error_code ec;
		std::filesystem::create_directories(settings_path, ec);
	}

	return settings_path;
}

boost::process::environment u::setup_vspipe_environment() {
	auto env = boost::this_process::environment();

#ifdef __APPLE__
	if (blur.used_installer) {
		env["PYTHONHOME"] = (blur.resources_path / "python").native();
		env["PYTHONPATH"] = (blur.resources_path / "python/lib/python3.12/site-packages").native();
	}
#endif

#ifdef __linux__
	auto app_config = config_app::get_app_config();
	if (!app_config.vapoursynth_lib_path.empty()) {
		env["LD_LIBRARY_PATH"] = app_config.vapoursynth_lib_path;
		env["PYTHONPATH"] = app_config.vapoursynth_lib_path + "/python3.12/site-packages";
	}
#endif

	return env;
}

u::VideoInfo u::get_video_info(const std::filesystem::path& path) {
	namespace bp = boost::process;

	bp::ipstream pipe_stream;

	auto c = u::run_command(
		blur.ffprobe_path,
		{
			"-v",
			"error",
			"-select_streams",
			"v:0", // only want to analyse first video stream
			"-show_entries",
			"stream=codec_type,codec_name,duration,color_range,sample_rate,r_frame_rate,pix_fmt,color_space,color_"
			"transfer,"
			"color_primaries",
			"-show_entries",
			"format=duration",
			"-of",
			"default=noprint_wrappers=1",
			u::path_to_string(path),
		},
		bp::std_out > pipe_stream,
		bp::std_err.null()
	);

	VideoInfo info;

	bool has_video_stream = false;
	std::string codec_name;

	std::string line;
	while (pipe_stream && std::getline(pipe_stream, line)) {
		boost::algorithm::trim(line);

		if (line.find("codec_type=video") != std::string::npos) {
			has_video_stream = true;
		}
		else if (line.find("codec_name=") != std::string::npos) {
			codec_name = line.substr(line.find('=') + 1);
		}
		else if (line.find("duration=") != std::string::npos) {
			try {
				info.duration = std::stod(line.substr(line.find('=') + 1));
			}
			catch (...) {
				info.duration = 0.0;
			}
		}
		else if (line.find("color_range=") != std::string::npos) {
			info.color_range = line.substr(line.find('=') + 1);
		}
		else if (line.find("pix_fmt=") != std::string::npos) {
			info.pix_fmt = line.substr(line.find('=') + 1);
		}
		else if (line.find("color_space=") != std::string::npos) {
			info.color_space = line.substr(line.find('=') + 1);
		}
		else if (line.find("color_transfer=") != std::string::npos) {
			info.color_transfer = line.substr(line.find('=') + 1);
		}
		else if (line.find("color_primaries=") != std::string::npos) {
			info.color_primaries = line.substr(line.find('=') + 1);
		}
		else if (line.find("sample_rate=") != std::string::npos) {
			info.sample_rate = std::stoi(line.substr(line.find('=') + 1));
		}
		else if (line.find("r_frame_rate=") != std::string::npos) {
			std::string frame_rate_str = line.substr(line.find('=') + 1);
			auto fps_split = u::split_string(frame_rate_str, "/");
			if (fps_split.size() == 2) {
				info.fps_num = std::stoi(fps_split[0]);
				info.fps_den = std::stoi(fps_split[1]);
			}
			else {
				// todo: throw? what??
			}
		}
	}

	c.wait();

	// 1. It must have a video stream
	// 2. Either it has a non-zero duration or it's an animated format
	// Static images will typically have duration=0 or N/A
	bool is_animated_format = u::contains(codec_name, "gif") || u::contains(codec_name, "webp");
	info.has_video_stream = has_video_stream && (info.duration > 0.1 || is_animated_format);

	if (info.sample_rate == -1) {
		// todo: throw?
	}

	return info;
}

std::vector<int16_t> u::get_video_waveform(const std::filesystem::path& path, int target_width) {
	namespace bp = boost::process;

	bp::ipstream pipe_stream;

	auto c = u::run_command(
		blur.ffmpeg_path,
		{
			"-v",
			"error",
			"-i",
			path.string(),
			"-f",
			"s16le",
			"-acodec",
			"pcm_s16le",
			"-ac",
			"1",
			"-ar",
			"44100",
			"-",
		},
		bp::std_out > pipe_stream,
		bp::std_err.null()
	);

	std::vector<char> buffer(4096);
	std::vector<int16_t> raw_samples;

	while (pipe_stream.read(buffer.data(), buffer.size()) || pipe_stream.gcount() > 0) {
		auto bytes_read = static_cast<size_t>(pipe_stream.gcount());

		if (bytes_read % 2 != 0)
			--bytes_read;

		size_t sample_count = bytes_read / 2;
		size_t old_size = raw_samples.size();
		raw_samples.resize(raw_samples.size() + sample_count);
		std::memcpy(&raw_samples[old_size], buffer.data(), bytes_read);
	}

	std::vector<int16_t> downsampled;
	if (!raw_samples.empty()) {
		size_t samples_per_pixel = std::max<size_t>(1, raw_samples.size() / target_width);

		for (size_t i = 0; i < raw_samples.size(); i += samples_per_pixel) {
			float sum = 0.0f;
			for (size_t j = i; j < std::min(i + samples_per_pixel, raw_samples.size()); ++j) {
				auto sample = static_cast<float>(raw_samples[j]);
				sum += sample * sample;
			}
			float rms = std::sqrt(sum / samples_per_pixel);
			downsampled.push_back(static_cast<int16_t>(rms));
		}
	}

	return downsampled;
}

int16_t u::get_audio_percentile_peak(const std::vector<int16_t>& samples, float percentile) {
	if (samples.empty())
		return 1;

	// sort samples from quietest->loudest
	std::vector<int16_t> abs_samples;
	abs_samples.reserve(samples.size());
	for (int16_t sample : samples) {
		abs_samples.push_back(std::abs(sample));
	}

	std::ranges::sort(abs_samples);

	// get xth percentile amplitude
	auto idx = static_cast<size_t>(percentile * abs_samples.size());
	idx = std::min(idx, abs_samples.size() - 1);
	return std::max(abs_samples[idx], static_cast<int16_t>(1));
}

bool u::test_hardware_device(const std::string& device_type) {
	namespace bp = boost::process;

	bp::ipstream error_stream;

	auto c = u::run_command(
		blur.ffmpeg_path,
		{
			"-init_hw_device",
			(device_type + "=hw"),
			"-loglevel",
			"error",
		},
		bp::std_out.null(),
		bp::std_err > error_stream
	);

	std::string line;
	if (std::getline(error_stream, line)) {
		// any error output means the device is not available
		c.terminate();
		return false;
	}

	c.wait();
	return true;
}

std::vector<u::EncodingDevice> u::get_hardware_encoding_devices() {
	static std::vector<EncodingDevice> devices;

	if (init_hw)
		return devices;
	else
		init_hw = true;

	struct HardwareTest {
		std::string type;
		std::string method;
		std::string ffmpeg_device_type;
	};

	std::vector<HardwareTest> tests = {
		// in order of priority
		// e.g. if you have nvidia + amd/intel you'll want to use nvidia over them i assume
		{ .type = "nvidia", .method = "nvenc", .ffmpeg_device_type = "cuda" },
		{ .type = "amd", .method = "amf", .ffmpeg_device_type = "d3d11va" },
		{ .type = "intel", .method = "qsv", .ffmpeg_device_type = "qsv" },
#ifdef __APPLE__
		{ .type = "mac", .method = "videotoolbox", .ffmpeg_device_type = "videotoolbox" }
#endif
	};

	std::vector<std::future<bool>> futures;
	futures.reserve(tests.size());

	for (const auto& test : tests) {
		futures.push_back(std::async(std::launch::async, [&test]() {
			return test_hardware_device(test.ffmpeg_device_type);
		}));
	}

	for (size_t i = 0; i < tests.size(); ++i) {
		if (futures[i].get()) {
			devices.emplace_back(
				EncodingDevice{
					.type = tests[i].type,
					.method = tests[i].method,
					.is_primary = devices.empty(),
				}
			);
		}
	}

	return devices;
}

std::vector<std::string> u::get_available_gpu_types() {
	auto devices = get_hardware_encoding_devices();
	std::vector<std::string> gpu_types;

	gpu_types.reserve(devices.size());
	for (const auto& device : devices) {
		gpu_types.push_back(device.type);
	}

	return gpu_types;
}

std::string u::get_primary_gpu_type() {
	auto devices = get_hardware_encoding_devices();

	// First try to find device marked as primary
	for (const auto& device : devices) {
		if (device.is_primary) {
			return device.type;
		}
	}

	// If no primary device found but we have devices, return the first one
	if (!devices.empty()) {
		return devices[0].type;
	}

	return "cpu";
}

std::vector<std::string> u::get_supported_presets(bool gpu_encoding, const std::string& gpu_type) {
	if (!init_hw)
		get_hardware_encoding_devices();

	auto available_presets = config_presets::get_available_presets(gpu_encoding, gpu_type);

	std::vector<std::string> filtered_presets;

	for (const auto& preset : available_presets) {
		// if (hw_encoders.contains(preset.codec)) {
		filtered_presets.push_back(preset.name);
		// }
	}

	return filtered_presets;
}

std::vector<std::string> u::ffmpeg_string_to_args(const std::string& str) {
	std::vector<std::string> args;

	bool in_quote = false;
	std::string current_arg;

	for (size_t i = 0; i < str.length(); i++) {
		char c = str[i];

		if (c == '"') {
			in_quote = !in_quote;
			// don't add the quote character to the argument
		}
		else if (c == ' ' && !in_quote) {
			if (!current_arg.empty()) {
				args.push_back(current_arg);
				current_arg.clear();
			}
		}
		else {
			current_arg += c;
		}
	}

	if (!current_arg.empty()) {
		args.push_back(current_arg);
	}

	return args;
}

std::map<int, std::string> u::get_rife_gpus() {
	namespace bp = boost::process;

	auto env = setup_vspipe_environment();

#if defined(__linux__)
	bool vapoursynth_plugins_bundled = std::filesystem::exists(blur.resources_path / "vapoursynth-plugins");
#endif

	std::filesystem::path get_gpus_script_path = (blur.resources_path / "lib/get_rife_gpus.py");

	bp::ipstream err_stream;

	auto c = u::run_command(
		blur.vspipe_path,
		{
			"-c",
			"y4m",
#if defined(__APPLE__)
			"-a",
			std::format("macos_bundled={}", blur.used_installer ? "true" : "false"),
#endif
#if defined(__linux__)
			"-a",
			std::format("linux_bundled={}", vapoursynth_plugins_bundled ? "true" : "false"),
#endif
			u::path_to_string(get_gpus_script_path),
			"-",
		},
		bp::std_out.null(),
		bp::std_err > err_stream,
		env
	);

	std::map<int, std::string> gpu_map;

	std::regex gpu_line_pattern(R"(\[(\d+)\s+(.*?)\])"); // regex to match: [0 GPU NAME]

	std::string line;
	while (err_stream && std::getline(err_stream, line)) {
		boost::algorithm::trim(line);

		std::smatch match;
		if (std::regex_search(line, match, gpu_line_pattern)) {
			int gpu_index = std::stoi(match[1].str());
			std::string gpu_name = match[2].str();

			gpu_map[gpu_index] = gpu_name;
		}
	}

	c.wait();

	return gpu_map;
}

int u::get_fastest_rife_gpu_index(
	const std::map<int, std::string>& gpu_map,
	const std::filesystem::path& rife_model_path,
	const std::filesystem::path& benchmark_video_path
) {
	namespace bp = boost::process;

	std::map<int, float> benchmark_map;
	float fastest_time = FLT_MAX;
	int fastest_index = -1;

	std::filesystem::path benchmark_gpus_script_path = (blur.resources_path / "lib/benchmark_rife_gpus.py");

	for (const auto& [gpu_index, gpu_name] : gpu_map) {
		auto env = setup_vspipe_environment();

#if defined(__linux__)
		bool vapoursynth_plugins_bundled = std::filesystem::exists(blur.resources_path / "vapoursynth-plugins");
#endif

		auto start = std::chrono::steady_clock::now();

		auto c = u::run_command(
			blur.vspipe_path,
			{
				"-c",
				"y4m",
				"-p",
				"-a",
				std::format("rife_model={}", rife_model_path),
				"-a",
				std::format("rife_gpu_index={}", gpu_index),
				"-a",
				std::format("benchmark_video_path={}", benchmark_video_path),
#if defined(__APPLE__)
				"-a",
				std::format("macos_bundled={}", blur.used_installer ? "true" : "false"),
#endif
#if defined(__linux__)
				"-a",
				std::format("linux_bundled={}", vapoursynth_plugins_bundled ? "true" : "false"),
#endif
#if defined(_WIN32)
				"-a",
				"enable_lsmash=true",
#endif
				"-e",
				"2",
				u::path_to_string(benchmark_gpus_script_path),
				"-",
			},
			env,
			bp::std_out.null(),
			bp::std_err.null()
		);

		bool killed_early = false;

		while (c.running()) {
			float elapsed_seconds =
				std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::steady_clock::now() - start)
					.count();

			if (elapsed_seconds > fastest_time) {
				c.terminate();
				killed_early = true;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}

		if (!killed_early) {
			float elapsed_seconds =
				std::chrono::duration_cast<std::chrono::duration<float>>(std::chrono::steady_clock::now() - start)
					.count();
			u::log("gpu {} took {}", gpu_index, elapsed_seconds);

			if (elapsed_seconds < fastest_time) {
				fastest_time = elapsed_seconds;
				fastest_index = gpu_index;
			}
		}
		else {
			u::log("gpu {} killed early (too slow)", gpu_index);
		}
	}

	return fastest_index;
}

void u::set_fastest_rife_gpu(BlurSettings& settings) {
	auto app_config = config_app::get_app_config();
	if (app_config.rife_gpu_index != -1)
		return;

	if (!blur.initialised_rife_gpus || blur.rife_gpus.empty())
		return;

	if (blur.rife_gpus.size() == 1) {
		// only one gpu, so it's the fastest. don't need to benchmark.
		app_config.rife_gpu_index = 0;
	}
	else {
		auto sample_video_path = blur.settings_path / "sample_video.mp4";
		if (!std::filesystem::exists(sample_video_path))
			return;

		auto rife_model_path = settings.get_rife_model_path();
		if (!rife_model_path)
			return;

		int fastest_gpu_index = u::get_fastest_rife_gpu_index(blur.rife_gpus, *rife_model_path, sample_video_path);
		app_config.rife_gpu_index = fastest_gpu_index;
	}

	// todo: this is dumb
	auto app_config_path = config_app::get_app_config_path();
	config_app::create(app_config_path, app_config);

	u::log("set rife_gpu_index to the fastest gpu ({})", app_config.rife_gpu_index);
}

void u::verify_gpu_encoding(BlurSettings& settings) {
	if (!blur.initialised)
		return;

	auto app_config = config_app::get_app_config();

	if (app_config.gpu_type.empty() || !u::contains(u::get_available_gpu_types(), app_config.gpu_type)) {
		app_config.gpu_type = u::get_primary_gpu_type();
	}

	if (app_config.gpu_type == "cpu") {
		settings.gpu_encoding = false;
	}

	auto available_codecs = u::get_supported_presets(settings.gpu_encoding, app_config.gpu_type);

	if (!u::contains(available_codecs, settings.encode_preset)) {
		settings.encode_preset = "h264";
	}

	// todo: this is dumb
	auto app_config_path = config_app::get_app_config_path();
	config_app::create(app_config_path, app_config);
}

#ifdef WIN32
bool u::windows_toggle_suspend_process(DWORD pid, bool to_suspend) {
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
		return false;

	THREADENTRY32 te;
	te.dwSize = sizeof(te);

	if (!Thread32First(hSnapshot, &te)) {
		CloseHandle(hSnapshot);
		return false;
	}

	do {
		if (te.th32OwnerProcessID == pid) {
			HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
			if (hThread) {
				if (to_suspend)
					SuspendThread(hThread);
				else
					ResumeThread(hThread);
				CloseHandle(hThread);
			}
		}
	}
	while (Thread32Next(hSnapshot, &te));

	CloseHandle(hSnapshot);
	return true;
}
#endif
