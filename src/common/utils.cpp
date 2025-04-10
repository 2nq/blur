#include "utils.h"
#include "common/config_presets.h"

namespace {
	bool init_hw = false;
	std::set<std::string> hw_accels;
	std::set<std::string> hw_encoders;
}

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

	std::filesystem::path path(program_path.string());

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

float u::lerp(float value, float target, float reset_speed, float snap_offset) {
	value = std::lerp(value, target, reset_speed);

	if (std::abs(value - target) < snap_offset) // todo: is this too small
		value = target;

	return value;
}

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

u::VideoInfo u::get_video_info(const std::filesystem::path& path) {
	namespace bp = boost::process;

	bp::environment env = boost::this_process::environment();

	if (blur.used_installer) {
#ifdef __linux__
		env["LD_LIBRARY_PATH"] = (blur.resources_path / "../lib").string();
#endif
	}

	bp::ipstream pipe_stream;
	bp::child c(
		blur.ffprobe_path.wstring(),
		"-v",
		"error",
		"-select_streams",
		"v:0", // only want to analyse first video stream
		"-show_entries",
		"stream=codec_type,codec_name,duration,color_range,sample_rate,r_frame_rate",
		"-show_entries",
		"format=duration",
		"-of",
		"default=noprint_wrappers=1",
		path.wstring(),
		bp::std_out > pipe_stream,
		bp::std_err > bp::null
#ifdef _WIN32
		,
		bp::windows::create_no_window
#endif
	);

	VideoInfo info;

	bool has_video_stream = false;
	double duration = 0.0;
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
				duration = std::stod(line.substr(line.find('=') + 1));
			}
			catch (...) {
				duration = 0.0;
			}
		}
		else if (line.find("color_range=") != std::string::npos) {
			info.color_range = line.substr(line.find('=') + 1);
		}
		else if (line.find("sample_rate=") != std::string::npos) {
			std::string sample_rate_str = line.substr(line.find('=') + 1);
			info.sample_rate = std::stoi(sample_rate_str);
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
	info.has_video_stream = has_video_stream && (duration > 0.1 || is_animated_format);

	if (info.sample_rate == -1) {
		// todo: throw?
	}

	return info;
}

std::vector<u::EncodingDevice> u::get_hardware_encoding_devices() {
	namespace bp = boost::process;

	static std::vector<EncodingDevice> devices;

	if (init_hw)
		return devices;
	else
		init_hw = true;

	bp::environment env = boost::this_process::environment();

	if (blur.used_installer) {
#ifdef __linux__
		env["LD_LIBRARY_PATH"] = (blur.resources_path / "../lib").string();
#endif
	}

	// First check available hardware acceleration methods
	bp::ipstream pipe_stream;
	bp::child c(
		blur.ffmpeg_path.wstring(),
		"-v",
		"error",
		"-hide_banner",
		"-hwaccels",
		bp::std_out > pipe_stream,
		bp::std_err > bp::null,
		env
#ifdef _WIN32
		,
		bp::windows::create_no_window
#endif
	);

	std::string line;

	bool in_accel_section = false;

	while (pipe_stream && std::getline(pipe_stream, line)) {
		boost::algorithm::trim(line);

		if (line == "Hardware acceleration methods:") {
			in_accel_section = true;
			continue;
		}

		if (in_accel_section && !line.empty()) {
			hw_accels.insert(line);
		}
	}

	c.wait();

	// Now check specific encoders
	bp::ipstream encoder_stream;
	bp::child c2(
		blur.ffmpeg_path.wstring(),
		"-v",
		"error",
		"-hide_banner",
		"-encoders",
		bp::std_out > encoder_stream,
		bp::std_err > bp::null
#ifdef _WIN32
		,
		bp::windows::create_no_window
#endif
	);

	bool in_encoder_section = false;

	while (encoder_stream && std::getline(encoder_stream, line)) {
		boost::algorithm::trim(line);

		if (line == "------") {
			in_encoder_section = true;
			continue;
		}

		if (in_encoder_section && !line.empty() && u::contains(line, "V....D")) {
			auto tmp = u::split_string(line, "V....D ")[1];
			auto encoder = u::split_string(tmp, " ")[0];
			hw_encoders.insert(encoder);
		}
	}

	c2.wait();

	// Check for NVIDIA (NVENC)
	bool has_nvidia = false;
	if (u::contains(hw_accels, "cuda") || u::contains(hw_accels, "nvenc") || u::contains(hw_accels, "nvdec")) {
		for (const auto& encoder : hw_encoders) {
			if (u::contains(encoder, "nvenc")) {
				EncodingDevice device;
				device.type = "nvidia";
				device.method = "nvenc";
				device.is_primary = true;
				devices.push_back(device);
				break;
			}
		}
	}

	// Check for AMD (AMF)
	bool has_amd = false;
	for (const auto& encoder : hw_encoders) {
		if (u::contains(encoder, "amf")) {
			has_amd = true;
			EncodingDevice device;
			device.type = "amd";
			device.method = "amf";
			device.is_primary = devices.empty(); // Primary if no other device yet
			devices.push_back(device);
			break;
		}
	}

	// Check for Intel (QSV)
	if (u::contains(hw_accels, "qsv")) {
		for (const auto& encoder : hw_encoders) {
			if (u::contains(encoder, "qsv")) {
				EncodingDevice device;
				device.type = "intel";
				device.method = "qsv";
				device.is_primary = devices.empty(); // Primary if no other device yet
				devices.push_back(device);
				break;
			}
		}
	}

	// Check for Mac (VideoToolbox)
	bool has_mac = false;
#ifdef __APPLE__
	for (const auto& encoder : hw_encoders) {
		if (u::contains(encoder, "videotoolbox")) {
			has_mac = true;
			EncodingDevice device;
			device.type = "mac";
			device.method = "videotoolbox";
			device.is_primary = devices.empty(); // Primary if no other device yet
			devices.push_back(device);
			break;
		}
	}
#endif

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
		if (hw_encoders.contains(preset.codec)) {
			filtered_presets.push_back(preset.name);
		}
	}

	return filtered_presets;
}

std::vector<std::wstring> u::ffmpeg_string_to_args(const std::wstring& str) {
	std::vector<std::wstring> args;

	bool in_quote = false;
	std::wstring current_arg;

	for (size_t i = 0; i < str.length(); i++) {
		wchar_t c = str[i];

		if (c == L'"') {
			in_quote = !in_quote;
			// don't add the quote character to the argument
		}
		else if (c == L' ' && !in_quote) {
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
#ifdef __APPLE__ // todo: rife mac issue
	return {};
#endif

	namespace bp = boost::process;

	bp::environment env = boost::this_process::environment();

#if defined(__APPLE__)
	if (blur.used_installer) {
		env["PYTHONHOME"] = (blur.resources_path / "python").string();
		env["PYTHONPATH"] = (blur.resources_path / "python/lib/python3.12/site-packages").string();
	}
#endif

	std::wstring get_gpus_script_path = (blur.resources_path / "lib/get_rife_gpus.py").wstring();

	bp::ipstream err_stream;

	bp::child c(
		blur.vspipe_path.wstring(),
		L"-c",
		L"y4m",
		get_gpus_script_path,
		L"-",
		bp::std_out.null(),
		bp::std_err > err_stream,
		env
#ifdef _WIN32
		,
		bp::windows::create_no_window
#endif
	);

	std::map<int, std::string> gpu_map;

	std::string line;
	std::regex gpu_line_pattern(R"(\[(\d+)\s+(.*?)\])"); // regex to match: [0 GPU NAME]

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
#ifdef __APPLE__ // todo: rife mac issue
	return 0;
#endif

	namespace bp = boost::process;

	std::map<int, float> benchmark_map;
	float fastest_time = FLT_MAX;
	int fastest_index = -1;

	std::wstring benchmark_gpus_script_path = (blur.resources_path / "lib/benchmark_rife_gpus.py").wstring();

	for (const auto& [gpu_index, gpu_name] : gpu_map) {
		bp::environment env = boost::this_process::environment();

#if defined(__APPLE__)
		if (blur.used_installer) {
			env["PYTHONHOME"] = (blur.resources_path / "python").string();
			env["PYTHONPATH"] = (blur.resources_path / "python/lib/python3.12/site-packages").string();
		}
#endif

		auto start = std::chrono::steady_clock::now();

		bp::child c(
			blur.vspipe_path.wstring(),
			L"-c",
			L"y4m",
			L"-p",
			L"-a",
			std::format(L"rife_model={}", rife_model_path.wstring()),
			L"-a",
			std::format(L"rife_gpu_index={}", gpu_index),
			L"-a",
			std::format(L"benchmark_video_path={}", benchmark_video_path.wstring()),
#if defined(__APPLE__)
			L"-a",
			std::format(L"macos_bundled={}", blur.used_installer ? L"true" : L"false"),
#endif
#if defined(_WIN32)
			L"-a",
			L"enable_lsmash=true",
#endif
			L"-e",
			L"2",
			benchmark_gpus_script_path,
			L"-",
			bp::std_out.null(),
			bp::std_err.null(),
			env
#ifdef _WIN32
			,
			bp::windows::create_no_window
#endif
		);

		c.detach();

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
