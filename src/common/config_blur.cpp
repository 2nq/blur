#include "config_blur.h"
#include "config_base.h"

void config_blur::create(const std::filesystem::path& filepath, const BlurSettings& current_settings) {
	std::ofstream output(filepath);

	output << "[blur v" << BLUR_VERSION << "]" << "\n";

	output << "\n";
	output << "- blur" << "\n";
	output << "blur: " << (current_settings.blur ? "true" : "false") << "\n";
	output << "blur amount: " << current_settings.blur_amount << "\n";
	output << "blur output fps: " << current_settings.blur_output_fps << "\n";
	output << "blur weighting: " << current_settings.blur_weighting << "\n";
	output << "blur gamma: " << current_settings.blur_gamma << "\n";

	output << "\n";
	output << "- interpolation" << "\n";
	output << "interpolate: " << (current_settings.interpolate ? "true" : "false") << "\n";
	output << "interpolated fps: " << current_settings.interpolated_fps << "\n";
#ifndef __APPLE__ // rife dont worky on mac (see renderer.cpp)
	output << "interpolation method: " << current_settings.interpolation_method << "\n";

	output << "\n";
	output << "- pre-interpolation" << "\n";
	output << "pre-interpolate: " << (current_settings.pre_interpolate ? "true" : "false") << "\n";
	output << "pre-interpolated fps: " << current_settings.pre_interpolated_fps << "\n";
#endif

	output << "\n";
	output << "- deduplication" << "\n";
	output << "deduplicate: " << (current_settings.deduplicate ? "true" : "false") << "\n";
	output << "deduplicate method: " << current_settings.deduplicate_method << "\n";

	output << "\n";
	output << "- rendering" << "\n";
	output << "encode preset: " << current_settings.encode_preset << "\n";
	output << "quality: " << current_settings.quality << "\n";
	output << "preview: " << (current_settings.preview ? "true" : "false") << "\n";
	output << "detailed filenames: " << (current_settings.detailed_filenames ? "true" : "false") << "\n";
	output << "copy dates: " << (current_settings.copy_dates ? "true" : "false") << "\n";

	output << "\n";
	output << "- gpu acceleration" << "\n";
	output << "gpu decoding: " << (current_settings.gpu_decoding ? "true" : "false") << "\n";
	output << "gpu interpolation: " << (current_settings.gpu_interpolation ? "true" : "false") << "\n";
	output << "gpu encoding: " << (current_settings.gpu_encoding ? "true" : "false") << "\n";
	output << "gpu type (nvidia/amd/intel): " << current_settings.gpu_type << "\n";
	output << "rife gpu number: " << current_settings.rife_gpu_index << "\n";

	output << "\n";
	output << "- timescale" << "\n";
	output << "timescale: " << (current_settings.timescale ? "true" : "false") << "\n";
	output << "input timescale: " << current_settings.input_timescale << "\n";
	output << "output timescale: " << current_settings.output_timescale << "\n";
	output << "adjust timescaled audio pitch: " << (current_settings.output_timescale_audio_pitch ? "true" : "false")
		   << "\n";

	output << "\n";
	output << "- filters" << "\n";
	output << "filters: " << (current_settings.filters ? "true" : "false") << "\n";
	output << "brightness: " << current_settings.brightness << "\n";
	output << "saturation: " << current_settings.saturation << "\n";
	output << "contrast: " << current_settings.contrast << "\n";

	output << "\n";
	output << "[advanced options]" << "\n";
	output << "advanced: " << (current_settings.override_advanced ? "true" : "false") << "\n";

	if (current_settings.override_advanced) {
		output << "\n";
		output << "- advanced deduplication" << "\n";
		output << "deduplicate range: " << current_settings.advanced.deduplicate_range << "\n";
		output << "deduplicate threshold: " << current_settings.advanced.deduplicate_threshold << "\n";

		output << "\n";
		output << "- advanced rendering" << "\n";
		output << "video container: " << current_settings.advanced.video_container << "\n";
		output << "custom ffmpeg filters: " << current_settings.advanced.ffmpeg_override << "\n";
		output << "debug: " << (current_settings.advanced.debug ? "true" : "false") << "\n";

		output << "\n";
		output << "- advanced blur" << "\n";
		output << "blur weighting gaussian std dev: " << current_settings.advanced.blur_weighting_gaussian_std_dev
			   << "\n";
		output << "blur weighting triangle reverse: "
			   << (current_settings.advanced.blur_weighting_triangle_reverse ? "true" : "false") << "\n";
		output << "blur weighting bound: " << current_settings.advanced.blur_weighting_bound << "\n";

		output << "\n";
		output << "- advanced interpolation" << "\n";
		output << "svp interpolation preset: " << current_settings.advanced.svp_interpolation_preset << "\n";
		output << "svp interpolation algorithm: " << current_settings.advanced.svp_interpolation_algorithm << "\n";
		output << "interpolation block size: " << current_settings.advanced.interpolation_blocksize << "\n";
		output << "interpolation mask area: " << current_settings.advanced.interpolation_mask_area << "\n";
#ifndef __APPLE__ // rife issue again
		output << "rife model: " << current_settings.advanced.rife_model << "\n";
#endif

		if (current_settings.advanced.manual_svp) {
			output << "\n";
			output << "- manual svp override" << "\n";
			output << "manual svp: " << (current_settings.advanced.manual_svp ? "true" : "false") << "\n";
			output << "super string: " << current_settings.advanced.super_string << "\n";
			output << "vectors string: " << current_settings.advanced.vectors_string << "\n";
			output << "smooth string: " << current_settings.advanced.smooth_string << "\n";
		}
	}
}

config_blur::ConfigValidationResponse config_blur::validate(BlurSettings& config, bool fix) {
	std::set<std::string> errors;

	if (!u::contains(SVP_INTERPOLATION_PRESETS, config.advanced.svp_interpolation_preset)) {
		errors.insert(
			std::format("SVP interpolation preset ({}) is not a valid option", config.advanced.svp_interpolation_preset)
		);

		if (fix)
			config.advanced.svp_interpolation_preset = DEFAULT_CONFIG.advanced.svp_interpolation_preset;
	}

	if (!u::contains(SVP_INTERPOLATION_ALGORITHMS, config.advanced.svp_interpolation_algorithm)) {
		errors.insert(
			std::format(
				"SVP interpolation algorithm ({}) is not a valid option", config.advanced.svp_interpolation_algorithm
			)
		);

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

	return ConfigValidationResponse{
		.success = errors.empty(),
		.error = u::join(errors, " "),
	};
}

BlurSettings config_blur::parse(const std::filesystem::path& config_filepath) {
	auto config_map = config_base::read_config_map(config_filepath);

	BlurSettings settings;

	config_base::extract_config_value(config_map, "blur", settings.blur);
	config_base::extract_config_value(config_map, "blur amount", settings.blur_amount);
	config_base::extract_config_value(config_map, "blur output fps", settings.blur_output_fps);
	config_base::extract_config_string(config_map, "blur weighting", settings.blur_weighting);
	config_base::extract_config_value(config_map, "blur gamma", settings.blur_gamma);

	config_base::extract_config_value(config_map, "interpolate", settings.interpolate);
	config_base::extract_config_string(config_map, "interpolated fps", settings.interpolated_fps);
#ifndef __APPLE__ // rife dont worky on mac (see renderer.cpp)
	config_base::extract_config_string(config_map, "interpolation method", settings.interpolation_method);

	config_base::extract_config_value(config_map, "pre-interpolate", settings.pre_interpolate);
	config_base::extract_config_string(config_map, "pre-interpolated fps", settings.pre_interpolated_fps);
#endif

	config_base::extract_config_value(config_map, "deduplicate", settings.deduplicate);
	config_base::extract_config_value(config_map, "deduplicate method", settings.deduplicate_method);

	config_base::extract_config_value(config_map, "encode preset", settings.encode_preset);
	config_base::extract_config_value(config_map, "quality", settings.quality);
	config_base::extract_config_value(config_map, "preview", settings.preview);
	config_base::extract_config_value(config_map, "detailed filenames", settings.detailed_filenames);
	config_base::extract_config_value(config_map, "copy dates", settings.copy_dates);

	config_base::extract_config_value(config_map, "gpu decoding", settings.gpu_decoding);
	config_base::extract_config_value(config_map, "gpu interpolation", settings.gpu_interpolation);
	config_base::extract_config_value(config_map, "gpu encoding", settings.gpu_encoding);
	config_base::extract_config_string(config_map, "gpu type (nvidia/amd/intel)", settings.gpu_type);
	config_base::extract_config_value(config_map, "rife gpu number", settings.rife_gpu_index);

	settings.verify_gpu_encoding();

	if (settings.rife_gpu_index == -1) {
		settings.set_fastest_rife_gpu();
	}

	config_base::extract_config_value(config_map, "timescale", settings.timescale);
	config_base::extract_config_value(config_map, "input timescale", settings.input_timescale);
	config_base::extract_config_value(config_map, "output timescale", settings.output_timescale);
	config_base::extract_config_value(
		config_map, "adjust timescaled audio pitch", settings.output_timescale_audio_pitch
	);

	config_base::extract_config_value(config_map, "filters", settings.filters);
	config_base::extract_config_value(config_map, "brightness", settings.brightness);
	config_base::extract_config_value(config_map, "saturation", settings.saturation);
	config_base::extract_config_value(config_map, "contrast", settings.contrast);

	config_base::extract_config_value(config_map, "advanced", settings.override_advanced);

	if (settings.override_advanced) {
		config_base::extract_config_value(config_map, "deduplicate range", settings.advanced.deduplicate_range);
		config_base::extract_config_string(
			config_map, "deduplicate threshold", settings.advanced.deduplicate_threshold
		);

		config_base::extract_config_value(config_map, "video container", settings.advanced.video_container);
		config_base::extract_config_string(config_map, "custom ffmpeg filters", settings.advanced.ffmpeg_override);
		config_base::extract_config_value(config_map, "debug", settings.advanced.debug);

		config_base::extract_config_value(
			config_map, "blur weighting gaussian std dev", settings.advanced.blur_weighting_gaussian_std_dev
		);
		config_base::extract_config_value(
			config_map, "blur weighting triangle reverse", settings.advanced.blur_weighting_triangle_reverse
		);
		config_base::extract_config_string(config_map, "blur weighting bound", settings.advanced.blur_weighting_bound);

		config_base::extract_config_string(
			config_map, "svp interpolation preset", settings.advanced.svp_interpolation_preset
		);
		config_base::extract_config_string(
			config_map, "svp interpolation algorithm", settings.advanced.svp_interpolation_algorithm
		);
		config_base::extract_config_string(
			config_map, "interpolation block size", settings.advanced.interpolation_blocksize
		);
		config_base::extract_config_value(
			config_map, "interpolation mask area", settings.advanced.interpolation_mask_area
		);
#ifndef __APPLE__ // rife issue again
		config_base::extract_config_string(config_map, "rife model", settings.advanced.rife_model);
#endif
		config_base::extract_config_value(config_map, "manual svp", settings.advanced.manual_svp);
		config_base::extract_config_string(config_map, "super string", settings.advanced.super_string);
		config_base::extract_config_string(config_map, "vectors string", settings.advanced.vectors_string);
		config_base::extract_config_string(config_map, "smooth string", settings.advanced.smooth_string);
	}

	// recreate the config file using the parsed values (keeps nice formatting)
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

BlurSettings::ToJsonResult BlurSettings::to_json() const {
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
	j["blur_weighting_triangle_reverse"] = this->advanced.blur_weighting_triangle_reverse;
	j["blur_weighting_bound"] = this->advanced.blur_weighting_bound;

	j["svp_interpolation_preset"] = this->advanced.svp_interpolation_preset;
	j["svp_interpolation_algorithm"] = this->advanced.svp_interpolation_algorithm;
	j["interpolation_blocksize"] = this->advanced.interpolation_blocksize;
	j["interpolation_mask_area"] = this->advanced.interpolation_mask_area;

	auto rife_model_path = get_rife_model_path();
	if (!rife_model_path.success) {
		return {
			.success = false,
			.error_message = rife_model_path.error_message,
		};
	}
	j["rife_model"] = *rife_model_path.rife_model_path;

	j["manual_svp"] = this->advanced.manual_svp;
	j["super_string"] = this->advanced.super_string;
	j["vectors_string"] = this->advanced.vectors_string;
	j["smooth_string"] = this->advanced.smooth_string;

	return {
		.success = true,
		.json = j,
	};
}

BlurSettings::BlurSettings() {
	verify_gpu_encoding();
}

auto& blur_copy = blur; // cause BlurSettings.blur is a thing

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

BlurSettings::GetRifeModelResult BlurSettings::get_rife_model_path() const {
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
		return {
			.success = false,
			.error_message = std::format("RIFE model '{}' could not be found", this->advanced.rife_model),
		};
#endif

	return {
		.success = true,
		.rife_model_path = rife_model_path,
	};
}

void BlurSettings::set_fastest_rife_gpu() {
	if (!blur_copy.initialised_rife_gpus)
		return;

	auto sample_video_path = blur_copy.settings_path / "sample_video.mp4";
	bool sample_video_exists = std::filesystem::exists(sample_video_path);

	if (sample_video_exists) {
		auto rife_model_path = this->get_rife_model_path();
		if (rife_model_path.success) {
			int fastest_gpu_index =
				u::get_fastest_rife_gpu_index(blur_copy.rife_gpus, *rife_model_path.rife_model_path, sample_video_path);

			this->rife_gpu_index = fastest_gpu_index;
			u::log("set rife_gpu_index to the fastest gpu ({})", fastest_gpu_index);
		}
	}
}
