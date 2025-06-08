#pragma once

struct PresetSettings {
	using CodecParams = std::vector<std::pair<std::string, std::string>>;
	using PresetMap = std::vector<std::pair<std::string, CodecParams>>;

	PresetMap presets = {
		{
			"nvidia",
			{
				{ "h264", "-c:v h264_nvenc -preset p6 -qp {quality}" },
				{ "h265", "-c:v hevc_nvenc -preset p6 -qp {quality}" },
				{ "av1", "-c:v av1_nvenc -preset p6 -qp {quality}" },
			},
		},
		{
			"amd",
			{
				{ "h264", "-c:v h264_amf -qp_i {quality} -qp_b {quality} -qp_p {quality} -quality quality" },
				{ "h265", "-c:v hevc_amf -qp_i {quality} -qp_b {quality} -qp_p {quality} -quality quality" },
				{ "av1", "-c:v av1_amf -qp_i {quality} -qp_b {quality} -qp_p {quality} -quality quality" },
			},
		},
		{
			"intel",
			{
				{ "h264", "-c:v h264_qsv -global_quality {quality} -preset veryslow" },
				{ "h265", "-c:v hevc_qsv -global_quality {quality} -preset veryslow" },
				{ "av1", "-c:v av1_qsv -global_quality {quality} -preset veryslow" },
			},
		},
		{
			"mac",
			{
				{ "h264", "-c:v h264_videotoolbox -q:v {quality} -allow_sw 0" },
				{ "h265", "-c:v hevc_videotoolbox -q:v {quality} -allow_sw 0" },
				{ "av1", "-c:v av1_videotoolbox -q:v {quality} -allow_sw 0" },
				{ "prores", "-c:v prores_videotoolbox -profile:v {quality} -allow_sw 0" },
			},
		},
		{
			"cpu",
			{
				{ "h264", "-c:v libx264 -pix_fmt yuv420p -preset superfast -crf {quality}" },
				{ "h265", "-c:v libx265 -pix_fmt yuv420p -preset medium -crf {quality}" },
				{ "av1", "-c:v libaom-av1 -pix_fmt yuv420p -cpu-used 4 -crf {quality}" },
				{ "vp9", "-c:v libvpx-vp9 -pix_fmt yuv420p -deadline realtime -crf {quality}" },
			},
		}
	};

	[[nodiscard]] const std::string* find_preset_params(const std::string& gpu_type, const std::string& preset_name)
		const {
		for (const auto& [type, codec_params] : presets) {
			if (type == gpu_type) {
				for (const auto& [codec, params] : codec_params) {
					if (codec == preset_name) {
						return &params;
					}
				}
				return nullptr;
			}
		}
		return nullptr;
	}

	[[nodiscard]] const CodecParams* find_preset_group(const std::string& gpu_type) const {
		for (const auto& [type, codec_params] : presets) {
			if (type == gpu_type) {
				return &codec_params;
			}
		}
		return nullptr;
	}

	bool operator==(const PresetSettings& other) const = default;
};

namespace config_presets {
	inline const PresetSettings DEFAULT_CONFIG;

	const std::string PRESET_CONFIG_FILENAME = "presets.toml";

	void create(const std::filesystem::path& filepath, const PresetSettings& current_settings = PresetSettings());

	tl::expected<void, std::string> validate(PresetSettings& config, bool fix);

	PresetSettings parse(const std::filesystem::path& config_filepath);

	std::filesystem::path get_preset_config_path();

	PresetSettings get_preset_config();

	struct PresetDetails {
		std::string name;
		std::string codec;
	};

	std::vector<PresetDetails> get_available_presets(bool gpu_encoding, const std::string& gpu_type);

	std::vector<std::wstring> get_preset_params(const std::string& gpu_type, const std::string& preset, int quality);
}
