#include "config_blur.h"
#include "config_base.h"
#include <toml++/toml.hpp>

void config_blur::create(const std::filesystem::path& filepath, const BlurSettings& current_settings) {
	toml::table config;

	// Version
	config.insert("version", "v" + BLUR_VERSION);

	// Blur section
	auto blur_table = toml::table{};
	blur_table.insert("blur", current_settings.blur);
	blur_table.insert("blur_amount", current_settings.blur_amount);
	blur_table.insert("blur_output_fps", current_settings.blur_output_fps);
	blur_table.insert("blur_weighting", current_settings.blur_weighting);
	blur_table.insert("blur_gamma", current_settings.blur_gamma);
	config.insert("blur", blur_table);

	// Interpolation section
	auto interpolation_table = toml::table{};
	interpolation_table.insert("interpolate", current_settings.interpolate);
	interpolation_table.insert("interpolated_fps", current_settings.interpolated_fps);
#ifndef __APPLE__
	interpolation_table.insert("interpolation_method", current_settings.interpolation_method);
#endif
	config.insert("interpolation", interpolation_table);

#ifndef __APPLE__
	// Pre-interpolation section
	auto pre_interpolation_table = toml::table{};
	pre_interpolation_table.insert("pre_interpolate", current_settings.pre_interpolate);
	pre_interpolation_table.insert("pre_interpolated_fps", current_settings.pre_interpolated_fps);
	config.insert("pre_interpolation", pre_interpolation_table);
#endif

	// Deduplication section
	auto deduplication_table = toml::table{};
	deduplication_table.insert("deduplicate", current_settings.deduplicate);
	deduplication_table.insert("deduplicate_method", current_settings.deduplicate_method);
	config.insert("deduplication", deduplication_table);

	// Rendering section
	auto rendering_table = toml::table{};
	rendering_table.insert("encode_preset", current_settings.encode_preset);
	rendering_table.insert("quality", current_settings.quality);
	rendering_table.insert("preview", current_settings.preview);
	rendering_table.insert("detailed_filenames", current_settings.detailed_filenames);
	rendering_table.insert("copy_dates", current_settings.copy_dates);
	config.insert("rendering", rendering_table);

	// GPU acceleration section
	auto gpu_table = toml::table{};
	gpu_table.insert("gpu_decoding", current_settings.gpu_decoding);
	gpu_table.insert("gpu_interpolation", current_settings.gpu_interpolation);
	gpu_table.insert("gpu_encoding", current_settings.gpu_encoding);
	gpu_table.insert("gpu_type", current_settings.gpu_type);
	gpu_table.insert("rife_gpu_number", current_settings.rife_gpu_index);
	config.insert("gpu", gpu_table);

	// Timescale section
	auto timescale_table = toml::table{};
	timescale_table.insert("timescale", current_settings.timescale);
	timescale_table.insert("input_timescale", current_settings.input_timescale);
	timescale_table.insert("output_timescale", current_settings.output_timescale);
	timescale_table.insert("adjust_timescaled_audio_pitch", current_settings.output_timescale_audio_pitch);
	config.insert("timescale", timescale_table);

	// Filters section
	auto filters_table = toml::table{};
	filters_table.insert("filters", current_settings.filters);
	filters_table.insert("brightness", current_settings.brightness);
	filters_table.insert("saturation", current_settings.saturation);
	filters_table.insert("contrast", current_settings.contrast);
	config.insert("filters", filters_table);

	// Advanced section
	auto advanced_table = toml::table{};
	advanced_table.insert("advanced", current_settings.override_advanced);
	config.insert("advanced", advanced_table);

	if (current_settings.override_advanced) {
		// Advanced deduplication
		auto advanced_deduplication_table = toml::table{};
		advanced_deduplication_table.insert("deduplicate_range", current_settings.advanced.deduplicate_range);
		advanced_deduplication_table.insert("deduplicate_threshold", current_settings.advanced.deduplicate_threshold);
		advanced_table.insert("deduplication", advanced_deduplication_table);

		// Advanced rendering
		auto advanced_rendering_table = toml::table{};
		advanced_rendering_table.insert("video_container", current_settings.advanced.video_container);
		advanced_rendering_table.insert("custom_ffmpeg_filters", current_settings.advanced.ffmpeg_override);
		advanced_rendering_table.insert("debug", current_settings.advanced.debug);
		advanced_table.insert("rendering", advanced_rendering_table);

		// Advanced blur
		auto advanced_blur_table = toml::table{};
		advanced_blur_table.insert(
			"blur_weighting_gaussian_std_dev", current_settings.advanced.blur_weighting_gaussian_std_dev
		);
		advanced_blur_table.insert(
			"blur_weighting_gaussian_mean", current_settings.advanced.blur_weighting_gaussian_mean
		);
		advanced_blur_table.insert(
			"blur_weighting_gaussian_bound", current_settings.advanced.blur_weighting_gaussian_bound
		);
		advanced_table.insert("blur", advanced_blur_table);

		// Advanced interpolation
		auto advanced_interpolation_table = toml::table{};
		advanced_interpolation_table.insert(
			"svp_interpolation_preset", current_settings.advanced.svp_interpolation_preset
		);
		advanced_interpolation_table.insert(
			"svp_interpolation_algorithm", current_settings.advanced.svp_interpolation_algorithm
		);
		advanced_interpolation_table.insert(
			"interpolation_block_size", current_settings.advanced.interpolation_blocksize
		);
		advanced_interpolation_table.insert(
			"interpolation_mask_area", current_settings.advanced.interpolation_mask_area
		);
#ifndef __APPLE__
		advanced_interpolation_table.insert("rife_model", current_settings.advanced.rife_model);
#endif
		advanced_table.insert("interpolation", advanced_interpolation_table);

		if (current_settings.advanced.manual_svp) {
			// Manual SVP override
			auto manual_svp_table = toml::table{};
			manual_svp_table.insert("manual_svp", current_settings.advanced.manual_svp);
			manual_svp_table.insert("super_string", current_settings.advanced.super_string);
			manual_svp_table.insert("vectors_string", current_settings.advanced.vectors_string);
			manual_svp_table.insert("smooth_string", current_settings.advanced.smooth_string);
			advanced_table.insert("svp", manual_svp_table);
		}
	}

	// GUI section
	auto gui_table = toml::table{};
	gui_table.insert("blur_amount_tied_to_fps", current_settings.blur_amount_tied_to_fps);
	config.insert("gui", gui_table);

	// Write to file
	std::ofstream output(filepath);
	output << config;
}

tl::expected<void, std::string> config_blur::validate(BlurSettings& config, bool fix) {
	std::set<std::string> errors;

	if (!u::contains(SVP_INTERPOLATION_PRESETS, config.advanced.svp_interpolation_preset)) {
		errors.insert(
			std::format("SVP interpolation preset ({}) is not a valid option", config.advanced.svp_interpolation_preset)
		);

		if (fix)
			config.advanced.svp_interpolation_preset = DEFAULT_CONFIG.advanced.svp_interpolation_preset;
	}

	if (!u::contains(SVP_INTERPOLATION_ALGORITHMS, config.advanced.svp_interpolation_algorithm)) {
		errors.insert(std::format(
			"SVP interpolation algorithm ({}) is not a valid option", config.advanced.svp_interpolation_algorithm
		));

		if (fix)
			config.advanced.svp_interpolation_algorithm = DEFAULT_CONFIG.advanced.svp_interpolation_algorithm;
	}

	if (!u::contains(INTERPOLATION_BLOCK_SIZES, config.advanced.interpolation_blocksize)) {
		errors.insert(
			std::format("Interpolation block size ({}) is not a valid option", config.advanced.interpolation_blocksize)
		);

		if (fix)
			config.advanced.interpolation_blocksize = DEFAULT_CONFIG.advanced.interpolation_blocksize;
	}

	if (!errors.empty())
		return tl::unexpected(u::join(errors, " "));

	return {};
}

BlurSettings config_blur::parse(const std::filesystem::path& config_filepath) {
	BlurSettings settings;

	try {
		toml::table config = toml::parse_file(config_filepath.string());

		// Extract values using the new template functions from config_base.h
		config_base::extract_toml_value(config, "blur.blur", settings.blur, DEFAULT_CONFIG.blur);
		config_base::extract_toml_value(config, "blur.blur_amount", settings.blur_amount, DEFAULT_CONFIG.blur_amount);
		config_base::extract_toml_value(
			config, "blur.blur_output_fps", settings.blur_output_fps, DEFAULT_CONFIG.blur_output_fps
		);
		config_base::extract_toml_value(
			config, "blur.blur_weighting", settings.blur_weighting, DEFAULT_CONFIG.blur_weighting
		);
		config_base::extract_toml_value(config, "blur.blur_gamma", settings.blur_gamma, DEFAULT_CONFIG.blur_gamma);

		config_base::extract_toml_value(
			config, "interpolation.interpolate", settings.interpolate, DEFAULT_CONFIG.interpolate
		);
		config_base::extract_toml_value(
			config, "interpolation.interpolated_fps", settings.interpolated_fps, DEFAULT_CONFIG.interpolated_fps
		);
#ifndef __APPLE__
		config_base::extract_toml_value(
			config,
			"interpolation.interpolation_method",
			settings.interpolation_method,
			DEFAULT_CONFIG.interpolation_method
		);

		config_base::extract_toml_value(
			config, "pre_interpolation.pre_interpolate", settings.pre_interpolate, DEFAULT_CONFIG.pre_interpolate
		);
		config_base::extract_toml_value(
			config,
			"pre_interpolation.pre_interpolated_fps",
			settings.pre_interpolated_fps,
			DEFAULT_CONFIG.pre_interpolated_fps
		);
#endif

		config_base::extract_toml_value(
			config, "deduplication.deduplicate", settings.deduplicate, DEFAULT_CONFIG.deduplicate
		);
		config_base::extract_toml_value(
			config, "deduplication.deduplicate_method", settings.deduplicate_method, DEFAULT_CONFIG.deduplicate_method
		);

		config_base::extract_toml_value(
			config, "rendering.encode_preset", settings.encode_preset, DEFAULT_CONFIG.encode_preset
		);
		config_base::extract_toml_value(config, "rendering.quality", settings.quality, DEFAULT_CONFIG.quality);
		config_base::extract_toml_value(config, "rendering.preview", settings.preview, DEFAULT_CONFIG.preview);
		config_base::extract_toml_value(
			config, "rendering.detailed_filenames", settings.detailed_filenames, DEFAULT_CONFIG.detailed_filenames
		);
		config_base::extract_toml_value(config, "rendering.copy_dates", settings.copy_dates, DEFAULT_CONFIG.copy_dates);

		config_base::extract_toml_value(config, "gpu.gpu_decoding", settings.gpu_decoding, DEFAULT_CONFIG.gpu_decoding);
		config_base::extract_toml_value(
			config, "gpu.gpu_interpolation", settings.gpu_interpolation, DEFAULT_CONFIG.gpu_interpolation
		);
		config_base::extract_toml_value(config, "gpu.gpu_encoding", settings.gpu_encoding, DEFAULT_CONFIG.gpu_encoding);
		config_base::extract_toml_value(config, "gpu.gpu_type", settings.gpu_type, DEFAULT_CONFIG.gpu_type);
		config_base::extract_toml_value(
			config, "gpu.rife_gpu_number", settings.rife_gpu_index, DEFAULT_CONFIG.rife_gpu_index
		);

		settings.verify_gpu_encoding();

		if (settings.rife_gpu_index == -1) {
			settings.set_fastest_rife_gpu();
		}

		config_base::extract_toml_value(config, "timescale.timescale", settings.timescale, DEFAULT_CONFIG.timescale);
		config_base::extract_toml_value(
			config, "timescale.input_timescale", settings.input_timescale, DEFAULT_CONFIG.input_timescale
		);
		config_base::extract_toml_value(
			config, "timescale.output_timescale", settings.output_timescale, DEFAULT_CONFIG.output_timescale
		);
		config_base::extract_toml_value(
			config,
			"timescale.adjust_timescaled_audio_pitch",
			settings.output_timescale_audio_pitch,
			DEFAULT_CONFIG.output_timescale_audio_pitch
		);

		config_base::extract_toml_value(config, "filters.filters", settings.filters, DEFAULT_CONFIG.filters);
		config_base::extract_toml_value(config, "filters.brightness", settings.brightness, DEFAULT_CONFIG.brightness);
		config_base::extract_toml_value(config, "filters.saturation", settings.saturation, DEFAULT_CONFIG.saturation);
		config_base::extract_toml_value(config, "filters.contrast", settings.contrast, DEFAULT_CONFIG.contrast);

		config_base::extract_toml_value(
			config, "advanced.advanced", settings.override_advanced, DEFAULT_CONFIG.override_advanced
		);

		if (settings.override_advanced) {
			config_base::extract_toml_value(
				config,
				"advanced.deduplication.deduplicate_range",
				settings.advanced.deduplicate_range,
				DEFAULT_CONFIG.advanced.deduplicate_range
			);
			config_base::extract_toml_value(
				config,
				"advanced.deduplication.deduplicate_threshold",
				settings.advanced.deduplicate_threshold,
				DEFAULT_CONFIG.advanced.deduplicate_threshold
			);

			config_base::extract_toml_value(
				config,
				"advanced.rendering.video_container",
				settings.advanced.video_container,
				DEFAULT_CONFIG.advanced.video_container
			);
			config_base::extract_toml_value(
				config,
				"advanced.rendering.custom_ffmpeg_filters",
				settings.advanced.ffmpeg_override,
				DEFAULT_CONFIG.advanced.ffmpeg_override
			);
			config_base::extract_toml_value(
				config, "advanced.rendering.debug", settings.advanced.debug, DEFAULT_CONFIG.advanced.debug
			);

			config_base::extract_toml_value(
				config,
				"advanced.blur.blur_weighting_gaussian_std_dev",
				settings.advanced.blur_weighting_gaussian_std_dev,
				DEFAULT_CONFIG.advanced.blur_weighting_gaussian_std_dev
			);
			config_base::extract_toml_value(
				config,
				"advanced.blur.blur_weighting_gaussian_mean",
				settings.advanced.blur_weighting_gaussian_mean,
				DEFAULT_CONFIG.advanced.blur_weighting_gaussian_mean
			);
			config_base::extract_toml_value(
				config,
				"advanced.blur.blur_weighting_gaussian_bound",
				settings.advanced.blur_weighting_gaussian_bound,
				DEFAULT_CONFIG.advanced.blur_weighting_gaussian_bound
			);

			config_base::extract_toml_value(
				config,
				"advanced.interpolation.svp_interpolation_preset",
				settings.advanced.svp_interpolation_preset,
				DEFAULT_CONFIG.advanced.svp_interpolation_preset
			);
			config_base::extract_toml_value(
				config,
				"advanced.interpolation.svp_interpolation_algorithm",
				settings.advanced.svp_interpolation_algorithm,
				DEFAULT_CONFIG.advanced.svp_interpolation_algorithm
			);
			config_base::extract_toml_value(
				config,
				"advanced.interpolation.interpolation_block_size",
				settings.advanced.interpolation_blocksize,
				DEFAULT_CONFIG.advanced.interpolation_blocksize
			);
			config_base::extract_toml_value(
				config,
				"advanced.interpolation.interpolation_mask_area",
				settings.advanced.interpolation_mask_area,
				DEFAULT_CONFIG.advanced.interpolation_mask_area
			);
#ifndef __APPLE__
			config_base::extract_toml_value(
				config,
				"advanced.interpolation.rife_model",
				settings.advanced.rife_model,
				DEFAULT_CONFIG.advanced.rife_model
			);
#endif
			config_base::extract_toml_value(
				config, "advanced.svp.manual_svp", settings.advanced.manual_svp, DEFAULT_CONFIG.advanced.manual_svp
			);
			config_base::extract_toml_value(
				config,
				"advanced.svp.super_string",
				settings.advanced.super_string,
				DEFAULT_CONFIG.advanced.super_string
			);
			config_base::extract_toml_value(
				config,
				"advanced.svp.vectors_string",
				settings.advanced.vectors_string,
				DEFAULT_CONFIG.advanced.vectors_string
			);
			config_base::extract_toml_value(
				config,
				"advanced.svp.smooth_string",
				settings.advanced.smooth_string,
				DEFAULT_CONFIG.advanced.smooth_string
			);
		}

		config_base::extract_toml_value(
			config,
			"gui.blur_amount_tied_to_fps",
			settings.blur_amount_tied_to_fps,
			DEFAULT_CONFIG.blur_amount_tied_to_fps
		);
	}
	catch (const toml::parse_error& err) {
		DEBUG_LOG("Error parsing TOML config file at %s: %s", config_filepath.string().c_str(), err.what());
		return BlurSettings(); // Return default settings on parse error
	}

	// Recreate the config file using the parsed values (keeps nice formatting)
	create(config_filepath, settings);

	return settings;
}

BlurSettings config_blur::parse_global_config() {
	return parse(get_global_config_path());
}

std::filesystem::path config_blur::get_global_config_path() {
	return blur.settings_path / CONFIG_FILENAME;
}

std::filesystem::path config_blur::get_config_filename(const std::filesystem::path& video_folder) {
	return video_folder / CONFIG_FILENAME;
}

BlurSettings config_blur::get_global_config() {
	return config_base::load_config<BlurSettings>(get_global_config_path(), create, parse);
}

BlurSettings config_blur::get_config(const std::filesystem::path& config_filepath, bool use_global) {
	bool local_cfg_exists = std::filesystem::exists(config_filepath);

	auto global_cfg_path = get_global_config_path();
	bool global_cfg_exists = std::filesystem::exists(global_cfg_path);

	std::filesystem::path cfg_path;
	if (use_global && !local_cfg_exists && global_cfg_exists) {
		cfg_path = global_cfg_path;

		if (blur.verbose)
			u::log("Using global config");
	}
	else {
		// check if the config file exists, if not, write the default values
		if (!local_cfg_exists) {
			create(config_filepath);

			u::log(L"Configuration file not found, default config generated at {}", config_filepath.wstring());
		}

		cfg_path = config_filepath;
	}

	return parse(cfg_path);
}

tl::expected<nlohmann::json, std::string> BlurSettings::to_json() const {
	nlohmann::json j;

	j["blur"] = this->blur;
	j["blur_amount"] = this->blur_amount;
	j["blur_output_fps"] = this->blur_output_fps;
	j["blur_weighting"] = this->blur_weighting;
	j["blur_gamma"] = this->blur_gamma;

	j["interpolate"] = this->interpolate;
	j["interpolated_fps"] = this->interpolated_fps;
	j["interpolation_method"] = this->interpolation_method;

	j["pre_interpolate"] = this->pre_interpolate;
	j["pre_interpolated_fps"] = this->pre_interpolated_fps;

	j["deduplicate"] = this->deduplicate;
	j["deduplicate_method"] = this->deduplicate_method;

	j["timescale"] = this->timescale;
	j["input_timescale"] = this->input_timescale;
	j["output_timescale"] = this->output_timescale;
	j["output_timescale_audio_pitch"] = this->output_timescale_audio_pitch;

	j["filters"] = this->filters;
	j["brightness"] = this->brightness;
	j["saturation"] = this->saturation;
	j["contrast"] = this->contrast;

	j["encode preset"] = this->encode_preset;
	j["quality"] = this->quality;
	j["preview"] = this->preview;
	j["detailed_filenames"] = this->detailed_filenames;
	// j["copy_dates"] = this->copy_dates;

	j["gpu_decoding"] = this->gpu_decoding;
	j["gpu_interpolation"] = this->gpu_interpolation;
	j["gpu_encoding"] = this->gpu_encoding;
	j["gpu_type"] = this->gpu_type;
	j["rife_gpu_index"] = this->rife_gpu_index;

	j["filters"] = this->filters;
	j["brightness"] = this->brightness;
	j["saturation"] = this->saturation;
	j["contrast"] = this->contrast;

	// advanced
	j["deduplicate_range"] = this->advanced.deduplicate_range;
	j["deduplicate_threshold"] = this->advanced.deduplicate_threshold;

	// j["video_container"] = this->advanced.video_container;
	// j["ffmpeg_override"] = this->advanced.ffmpeg_override;
	j["debug"] = this->advanced.debug;

	j["blur_weighting_gaussian_std_dev"] = this->advanced.blur_weighting_gaussian_std_dev;
	j["blur_weighting_gaussian_mean"] = this->advanced.blur_weighting_gaussian_mean;
	j["blur_weighting_gaussian_bound"] = this->advanced.blur_weighting_gaussian_bound;

	j["svp_interpolation_preset"] = this->advanced.svp_interpolation_preset;
	j["svp_interpolation_algorithm"] = this->advanced.svp_interpolation_algorithm;
	j["interpolation_blocksize"] = this->advanced.interpolation_blocksize;
	j["interpolation_mask_area"] = this->advanced.interpolation_mask_area;

	auto rife_model_path = get_rife_model_path();
	if (!rife_model_path)
		return tl::unexpected(rife_model_path.error());

	j["rife_model"] = *rife_model_path;

	j["manual_svp"] = this->advanced.manual_svp;
	j["super_string"] = this->advanced.super_string;
	j["vectors_string"] = this->advanced.vectors_string;
	j["smooth_string"] = this->advanced.smooth_string;

	return j;
}

BlurSettings::BlurSettings() {
	verify_gpu_encoding();
}

namespace {
	auto& blur_copy = blur; // cause BlurSettings.blur is a thing
}

void BlurSettings::verify_gpu_encoding() {
	if (!blur_copy.initialised)
		return;

	if (gpu_type.empty() || !u::contains(u::get_available_gpu_types(), gpu_type)) {
		gpu_type = u::get_primary_gpu_type();
	}

	if (gpu_type == "cpu") {
		gpu_encoding = false;
	}

	auto available_codecs = u::get_supported_presets(gpu_encoding, gpu_type);

	if (!u::contains(available_codecs, encode_preset)) {
		encode_preset = "h264";
	}
}

// NOLINTBEGIN(readability-convert-member-functions-to-static) other platforms need it
tl::expected<std::filesystem::path, std::string> BlurSettings::get_rife_model_path() const {
	// NOLINTEND(readability-convert-member-functions-to-static)
	std::filesystem::path rife_model_path;

#ifndef __APPLE__ // rife issue again
#	if defined(_WIN32)
	rife_model_path = u::get_resources_path() / "lib/models" / this->advanced.rife_model;
#	elif defined(__linux__)
	rife_model_path = u::get_resources_path() / "models" / this->advanced.rife_model;
#	elif defined(__APPLE__)
	rife_model_path = u::get_resources_path() / "models" / this->advanced.rife_model;
#	endif

	if (!std::filesystem::exists(rife_model_path))
		return tl::unexpected(std::format("RIFE model '{}' could not be found", this->advanced.rife_model));
#endif

	return rife_model_path;
}

void BlurSettings::set_fastest_rife_gpu() {
	if (!blur_copy.initialised_rife_gpus)
		return;

	auto sample_video_path = blur_copy.settings_path / "sample_video.mp4";
	bool sample_video_exists = std::filesystem::exists(sample_video_path);

	if (sample_video_exists) {
		auto rife_model_path = BlurSettings::get_rife_model_path();

		if (rife_model_path) {
			int fastest_gpu_index =
				u::get_fastest_rife_gpu_index(blur_copy.rife_gpus, *rife_model_path, sample_video_path);

			this->rife_gpu_index = fastest_gpu_index;
			u::log("set rife_gpu_index to the fastest gpu ({})", fastest_gpu_index);
		}
	}
}
