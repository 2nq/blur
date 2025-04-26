#pragma once

struct AdvancedSettings {
	std::string video_container = "mp4";
	int deduplicate_range = 2;
	std::string deduplicate_threshold = "0.001";
	std::string ffmpeg_override;
	bool debug = false;

	float blur_weighting_gaussian_std_dev = 2.f;
	bool blur_weighting_triangle_reverse = false;
	std::string blur_weighting_bound = "[0, 2]";

	std::string svp_interpolation_preset = "weak";
	std::string svp_interpolation_algorithm = "13";
	std::string interpolation_blocksize = "8";
	int interpolation_mask_area = 0;
	std::string rife_model = "rife-v4.26_ensembleFalse";

	bool manual_svp = false;
	std::string super_string;
	std::string vectors_string;
	std::string smooth_string;

	bool operator==(const AdvancedSettings& other) const {
		// reflection would be nice c++ -_-
		// todo: boost? i mean, im already using it partially
		return video_container == other.video_container && deduplicate_range == other.deduplicate_range &&
		       deduplicate_threshold == other.deduplicate_threshold && ffmpeg_override == other.ffmpeg_override &&
		       debug == other.debug && blur_weighting_gaussian_std_dev == other.blur_weighting_gaussian_std_dev &&
		       blur_weighting_triangle_reverse == other.blur_weighting_triangle_reverse &&
		       blur_weighting_bound == other.blur_weighting_bound &&
		       svp_interpolation_preset == other.svp_interpolation_preset &&
		       svp_interpolation_algorithm == other.svp_interpolation_algorithm &&
		       interpolation_blocksize == other.interpolation_blocksize &&
		       interpolation_mask_area == other.interpolation_mask_area && rife_model == other.rife_model &&
		       manual_svp == other.manual_svp && super_string == other.super_string &&
		       vectors_string == other.vectors_string && smooth_string == other.smooth_string;
	}
};

struct BlurSettings {
	bool blur = true;
	float blur_amount = 1.f;
	int blur_output_fps = 60;
	std::string blur_weighting = "equal";

	bool interpolate = true;
	std::string interpolated_fps = "5x";
	std::string interpolation_method = "svp";

	bool pre_interpolate = false;
	std::string pre_interpolated_fps = "360";

	bool timescale = false;
	float input_timescale = 1.f;
	float output_timescale = 1.f;
	bool output_timescale_audio_pitch = false;

	bool filters = false;
	float brightness = 1.f;
	float saturation = 1.f;
	float contrast = 1.f;

	std::string encode_preset = "h264";
	int quality = 16;

	bool deduplicate = true;
	std::string deduplicate_method = "svp";

	bool preview = true;
	bool detailed_filenames = false;
	bool copy_dates = false;

	bool gpu_decoding = true;
	bool gpu_interpolation = true;
	bool gpu_encoding = true;
	std::string gpu_type = "";
	int rife_gpu_index = -1;

	bool override_advanced = false;
	AdvancedSettings advanced;

public:
	BlurSettings();

	bool operator==(const BlurSettings& other) const {
		// reflection would be nice c++ -_-
		// todo: boost? i mean, im already using it partially
		return blur == other.blur && blur_amount == other.blur_amount && blur_output_fps == other.blur_output_fps &&
		       blur_weighting == other.blur_weighting && interpolate == other.interpolate &&
		       interpolated_fps == other.interpolated_fps && interpolation_method == other.interpolation_method &&
		       pre_interpolate == other.pre_interpolate && pre_interpolated_fps == other.pre_interpolated_fps &&
		       timescale == other.timescale && input_timescale == other.input_timescale &&
		       output_timescale == other.output_timescale &&
		       output_timescale_audio_pitch == other.output_timescale_audio_pitch && filters == other.filters &&
		       brightness == other.brightness && saturation == other.saturation && contrast == other.contrast &&
		       encode_preset == other.encode_preset && deduplicate == other.deduplicate &&
		       deduplicate_method == other.deduplicate_method && quality == other.quality && preview == other.preview &&
		       detailed_filenames == other.detailed_filenames && copy_dates == other.copy_dates &&
		       gpu_decoding == other.gpu_decoding && gpu_interpolation == other.gpu_interpolation &&
		       gpu_encoding == other.gpu_encoding && gpu_type == other.gpu_type &&
		       rife_gpu_index == other.rife_gpu_index && override_advanced == other.override_advanced &&
		       advanced == other.advanced;
	}

	void verify_gpu_encoding();

	struct ToJsonResult {
		bool success;
		std::optional<nlohmann::json> json;
		std::string error_message;
	};

	[[nodiscard]] ToJsonResult to_json() const;

	struct GetRifeModelResult {
		bool success;
		std::optional<std::filesystem::path> rife_model_path;
		std::string error_message;
	};

	[[nodiscard]] GetRifeModelResult get_rife_model_path() const;
	void set_fastest_rife_gpu();
};

namespace config_blur {
	inline const BlurSettings DEFAULT_CONFIG;

	inline const std::vector<std::string> SVP_INTERPOLATION_PRESETS = {
		"weak", "film", "smooth", "animation", "default", "test",
	};

	inline const std::vector<std::string> SVP_INTERPOLATION_ALGORITHMS = {
		"1", "2", "11", "13", "21", "23",
	};

	inline const std::vector<std::string> INTERPOLATION_BLOCK_SIZES = { "4", "8", "16", "32" };

	const std::string CONFIG_FILENAME = ".blur-config.cfg";

	void create(const std::filesystem::path& filepath, const BlurSettings& current_settings = BlurSettings());

	struct ConfigValidationResponse {
		bool success;
		std::string error;
	};

	ConfigValidationResponse validate(BlurSettings& config, bool fix);

	BlurSettings parse(const std::filesystem::path& config_filepath);
	BlurSettings parse_global_config();

	std::filesystem::path get_global_config_path();
	std::filesystem::path get_config_filename(const std::filesystem::path& video_folder);
	BlurSettings get_global_config();
	BlurSettings get_config(const std::filesystem::path& config_filepath, bool use_global);
}
