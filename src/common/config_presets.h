#pragma once

struct PresetSettings {
	struct CodecParams {
		std::string codec;
		std::string params;
	};

	struct PresetCodecs {
		std::string type;
		std::vector<CodecParams> codec_params;
	};

	using PresetMap = std::vector<PresetCodecs>;

	struct Migration {
		std::string old_params;
		std::string new_params;
	};

	using CodecMigrationMap = std::map<std::string, Migration>;
	using MigrationMap = std::vector<std::map<std::string, CodecMigrationMap>>;

	PresetMap presets = {
		{
			.type="nvidia",
			.codec_params={
				{ .codec = "h264", .params = "-c:v h264_nvenc -preset p6 -qp {quality}" },
				{ .codec = "h265", .params = "-c:v hevc_nvenc -preset p6 -qp {quality}" },
				{ .codec = "av1", .params = "-c:v av1_nvenc -preset p6 -qp {quality}" },
			},
		},
		{
			.type="amd",
			.codec_params={
				{ .codec = "h264",
		          .params = "-c:v h264_amf -qp_i {quality} -qp_b {quality} -qp_p {quality} -quality quality" },
				{ .codec = "h265",
		          .params = "-c:v hevc_amf -qp_i {quality} -qp_b {quality} -qp_p {quality} -quality quality" },
				{ .codec = "av1",
		          .params = "-c:v av1_amf -qp_i {quality} -qp_b {quality} -qp_p {quality} -quality quality" },
			},
		},
		{
			.type="intel",
			.codec_params={
				{ .codec = "h264", .params = "-c:v h264_qsv -global_quality {quality} -preset veryslow" },
				{ .codec = "h265", .params = "-c:v hevc_qsv -global_quality {quality} -preset veryslow" },
				{ .codec = "av1", .params = "-c:v av1_qsv -global_quality {quality} -preset veryslow" },
			},
		},
		{
			.type="mac",
			.codec_params={
				{ .codec = "h264", .params = "-c:v h264_videotoolbox -q:v {quality} -allow_sw 0" },
				{ .codec = "h265", .params = "-c:v hevc_videotoolbox -q:v {quality} -allow_sw 0" },
				{ .codec = "av1", .params = "-c:v av1_videotoolbox -q:v {quality} -allow_sw 0" },
				{ .codec = "prores", .params = "-c:v prores_videotoolbox -profile:v {quality} -allow_sw 0" },
			},
		},
		{
			.type="cpu",
			.codec_params={
				{ .codec = "h264", .params = "-c:v libx264 -pix_fmt yuv420p -preset superfast -crf {quality}" },
				{ .codec = "h265", .params = "-c:v libx265 -pix_fmt yuv420p -preset medium -crf {quality}" },
				{ .codec = "av1", .params = "-c:v libaom-av1 -pix_fmt yuv420p -cpu-used 4 -crf {quality}" },
				{ .codec = "vp9", .params = "-c:v libvpx-vp9 -pix_fmt yuv420p -deadline realtime -crf {quality}" },
			},
		}
	};

	static inline const MigrationMap MIGRATIONS = {
		{
			// 2.32 yuv420p removal
			{
				"cpu",
				{
					{
						"h264",
						{
							.old_params = "-c:v libx264 -pix_fmt yuv420p -preset superfast -crf {quality}",
							.new_params = "-c:v libx264 -preset superfast -crf {quality}",
						},
					},
					{
						"h265",
						{
							.old_params = "-c:v libx265 -pix_fmt yuv420p -preset medium -crf {quality}",
							.new_params = "-c:v libx265 -preset medium -crf {quality}",
						},
					},
					{
						"av1",
						{
							.old_params = "-c:v libaom-av1 -pix_fmt yuv420p -cpu-used 4 -crf {quality}",
							.new_params = "-c:v libaom-av1 -cpu-used 4 -crf {quality}",
						},
					},
					{
						"vp9",
						{
							.old_params = "-c:v libvpx-vp9 -pix_fmt yuv420p -deadline realtime -crf {quality}",
							.new_params = "-c:v libvpx-vp9 -deadline realtime -crf {quality}",
						},
					},
				},
			},
		},
	};

	[[nodiscard]] const std::string* find_preset_params(const std::string& gpu_type, const std::string& preset_name)
		const {
		for (const auto& preset : presets) {
			if (preset.type == gpu_type) {
				for (const auto& codec_params : preset.codec_params) {
					if (codec_params.codec == preset_name) {
						return &codec_params.params;
					}
				}
				return nullptr;
			}
		}
		return nullptr;
	}

	[[nodiscard]] const std::vector<CodecParams>* find_preset_group(const std::string& gpu_type) const {
		for (const auto& preset : presets) {
			if (preset.type == gpu_type) {
				return &preset.codec_params;
			}
		}
		return nullptr;
	}

	void migrate() { // jank but fuck it, works for now
		for (const auto& migration_map : MIGRATIONS) {
			for (auto& preset : this->presets) {
				if (!migration_map.contains(preset.type))
					continue;

				const auto& migration = migration_map.at(preset.type);

				for (auto& codec_params : preset.codec_params) {
					if (!migration.contains(codec_params.codec))
						continue;

					const auto& migration_rules = migration.at(codec_params.codec);

					if (migration_rules.old_params != codec_params.params)
						continue;

					codec_params.params = migration_rules.new_params;
					u::log("migrated {} {} to new", preset.type, codec_params.codec);
				}
			}
		}
	}
};

namespace config_presets {
	inline const PresetSettings DEFAULT_CONFIG;

	const std::string PRESET_CONFIG_FILENAME = "presets.cfg";

	void create(const std::filesystem::path& filepath, const PresetSettings& current_settings = PresetSettings());

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
