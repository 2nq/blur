#include "config_presets.h"
#include "config_base.h"

namespace {
	std::vector<std::wstring> get_ffmpeg_args(std::string params_str, int quality) {
		// replace quality placeholder
		params_str = u::replace_all(params_str, "{quality}", std::to_string(quality));

		return u::ffmpeg_string_to_args(u::towstring(params_str));
	}
}

void config_presets::create(const std::filesystem::path& filepath, const PresetSettings& current_settings) {
	std::ofstream output(filepath);

	output << "[blur v" << BLUR_VERSION << "]" << "\n";

	for (const auto& [gpu_type, preset_map] : current_settings.presets) {
		output << "\n";
		output << "- " << gpu_type << "\n";

		for (const auto& [preset_name, preset_params] : preset_map) {
			output << preset_name << ": " << preset_params << "\n";
		}
	}
}

PresetSettings config_presets::parse(const std::filesystem::path& config_filepath) {
	PresetSettings settings = DEFAULT_CONFIG;

	std::ifstream file(config_filepath);
	if (!file)
		return settings; // defaults if file couldn't be opened

	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	std::istringstream stream(content);
	std::string line;
	std::string current_gpu_type;
	PresetSettings::CodecParams* current_codec_params = nullptr;

	while (std::getline(stream, line)) {
		line = u::trim(line);

		if (line.empty() || line.front() == '[' || line.front() == '#') {
			continue;
		}

		if (line.front() == '-') {
			current_gpu_type = u::trim(line.substr(1));

			current_codec_params = nullptr;
			for (auto& [gpu_name, params] : settings.presets) {
				if (gpu_name == current_gpu_type) {
					current_codec_params = &params;
					break;
				}
			}

			// new gpu type
			if (!current_codec_params) {
				settings.presets.push_back({ current_gpu_type, {} });
				current_codec_params = &settings.presets.back().second;
			}

			continue;
		}

		size_t delimiter_pos = line.find(':');
		if (delimiter_pos != std::string::npos && current_codec_params) {
			std::string preset_name = u::trim(line.substr(0, delimiter_pos));
			std::string preset_params = u::trim(line.substr(delimiter_pos + 1));

			bool found = false;
			for (auto& [name, params] : *current_codec_params) {
				if (name == preset_name) {
					params = preset_params; // already exists - update
					found = true;
					break;
				}
			}

			// new preset
			if (!found) {
				current_codec_params->push_back({ preset_name, preset_params });
			}
		}
	}

	// recreate the config file
	create(config_filepath, settings);

	return settings;
}

std::filesystem::path config_presets::get_preset_config_path() {
	return blur.settings_path / PRESET_CONFIG_FILENAME;
}

PresetSettings config_presets::get_preset_config() {
	using namespace std::chrono;
	static constexpr auto reload_interval = seconds(5);
	static PresetSettings cached;
	static auto last_reload_time = steady_clock::now() - reload_interval;

	auto now = steady_clock::now();
	if (now - last_reload_time >= reload_interval) {
		cached = config_base::load_config<PresetSettings>(get_preset_config_path(), create, parse);
		last_reload_time = now;
	}

	return cached;
}

std::vector<config_presets::PresetDetails> config_presets::get_available_presets(
	bool gpu_encoding, const std::string& gpu_type
) {
	std::vector<PresetDetails> available_presets;

	std::string type_to_check = gpu_encoding ? gpu_type : "cpu";

	PresetSettings config = get_preset_config();
	const auto* preset_group = config.find_preset_group(type_to_check);

	if (preset_group) {
		for (const auto& [preset_name, params_str] : *preset_group) {
			auto params = get_ffmpeg_args(params_str, 0);

			for (auto it = params.rbegin(); it != params.rend(); ++it) {
				if (it == params.rbegin())
					continue;

				if (*it == L"-c:v" || *it == L"-codec:v") {
					std::wstring codec = *(it - 1);
					available_presets.push_back(
						{
							.name = preset_name,
							.codec = u::tostring(codec),
						}
					);
					break;
				}
			}
		}
	}

	return available_presets;
}

std::vector<std::wstring> config_presets::get_preset_params(
	const std::string& gpu_type, const std::string& preset, int quality
) {
	PresetSettings config = get_preset_config();
	const std::string* params_ptr = config.find_preset_params(gpu_type, preset);

	if (params_ptr) {
		return get_ffmpeg_args(*params_ptr, quality);
	}

	if (gpu_type != "cpu") {
		return get_preset_params("cpu", preset, quality);
	}

	return get_preset_params("cpu", "h264", quality);
}
