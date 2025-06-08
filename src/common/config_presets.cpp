#include "config_presets.h"
#include "config_base.h"
#include <toml++/toml.hpp>

namespace {
	std::vector<std::wstring> get_ffmpeg_args(std::string params_str, int quality) {
		// replace quality placeholder
		params_str = u::replace_all(params_str, "{quality}", std::to_string(quality));

		return u::ffmpeg_string_to_args(u::towstring(params_str));
	}
}

void config_presets::create(const std::filesystem::path& filepath, const PresetSettings& current_settings) {
	toml::table config;

	// Version
	config.insert("version", "v" + BLUR_VERSION);

	// Create sections for each GPU type
	for (const auto& [gpu_type, codec_params] : current_settings.presets) {
		auto gpu_table = toml::table{};

		for (const auto& [codec_name, params] : codec_params) {
			gpu_table.insert(codec_name, params);
		}

		config.insert(gpu_type, gpu_table);
	}

	// Write to file
	std::ofstream output(filepath);
	output << config;
}

tl::expected<void, std::string> config_presets::validate(PresetSettings& config, bool fix) {
	std::set<std::string> errors;

	// Validate that each GPU type has at least one preset
	for (const auto& [gpu_type, codec_params] : config.presets) {
		if (codec_params.empty()) {
			errors.insert(std::format("GPU type '{}' has no presets defined", gpu_type));

			if (fix) {
				// Add default h264 preset if missing
				auto& presets = const_cast<PresetSettings::CodecParams&>(codec_params);
				presets.emplace_back("h264", "-c:v libx264 -pix_fmt yuv420p -preset medium -crf {quality}");
			}
		}
	}

	// Validate that essential presets exist
	const std::vector<std::string> essential_gpu_types = { "cpu", "nvidia", "amd", "intel" };
	for (const std::string& gpu_type : essential_gpu_types) {
		bool found = false;
		for (const auto& [type, _] : config.presets) {
			if (type == gpu_type) {
				found = true;
				break;
			}
		}

		if (!found) {
			errors.insert(std::format("Essential GPU type '{}' is missing", gpu_type));

			if (fix) {
				// Add missing GPU type with default preset from DEFAULT_CONFIG
				const auto* default_params = DEFAULT_CONFIG.find_preset_group(gpu_type);
				if (default_params) {
					config.presets.emplace_back(gpu_type, *default_params);
				}
			}
		}
	}

	if (!errors.empty())
		return tl::unexpected(u::join(errors, " "));

	return {};
}

PresetSettings config_presets::parse(const std::filesystem::path& config_filepath) {
	PresetSettings settings;

	try {
		toml::table config = toml::parse_file(config_filepath.string());

		// Clear default presets
		settings.presets.clear();

		// Parse each GPU type section
		for (const auto& [key, value] : config) {
			std::string gpu_type = std::string(key.str());

			// Skip version and other non-GPU sections
			if (gpu_type == "version") {
				continue;
			}

			if (value.is_table()) {
				const auto* gpu_table = value.as_table();
				PresetSettings::CodecParams codec_params;

				for (const auto& [codec_key, codec_value] : *gpu_table) {
					std::string codec_name = std::string(codec_key.str());

					if (codec_value.is_string()) {
						std::string params = std::string(codec_value.as_string()->get());
						codec_params.emplace_back(codec_name, params);
					}
				}

				if (!codec_params.empty()) {
					settings.presets.emplace_back(gpu_type, std::move(codec_params));
				}
			}
		}

		// If no presets were loaded, use defaults
		if (settings.presets.empty()) {
			settings = DEFAULT_CONFIG;
		}
	}
	catch (const toml::parse_error& err) {
		DEBUG_LOG("Error parsing TOML preset config file at %s: %s", config_filepath.string().c_str(), err.what());
		return DEFAULT_CONFIG; // Return default settings on parse error
	}

	// Recreate the config file using the parsed values (keeps nice formatting)
	create(config_filepath, settings);

	return settings;
}

std::filesystem::path config_presets::get_preset_config_path() {
	return blur.settings_path / PRESET_CONFIG_FILENAME;
}

PresetSettings config_presets::get_preset_config() {
	return config_base::load_config<PresetSettings>(get_preset_config_path(), create, parse);
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
					available_presets.push_back({
						.name = preset_name,
						.codec = u::tostring(codec),
					});
					break;
				}
			}
		}
	}

	return available_presets;
}

// NOLINTBEGIN(misc-no-recursion) trust me bro
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

// NOLINTEND(misc-no-recursion)
