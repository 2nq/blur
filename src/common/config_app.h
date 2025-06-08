#pragma once

struct GlobalAppSettings {
	bool render_success_notifications = false;
	bool render_failure_notifications = false;

	bool check_updates = true;
	bool check_beta = false;

#ifdef __linux__
	std::string vapoursynth_lib_path;
#endif

	bool operator==(const GlobalAppSettings& other) const = default;
};

namespace config_app {
	const std::string APP_CONFIG_FILENAME = "app.toml";
	inline const GlobalAppSettings DEFAULT_APP_CONFIG;

	void create(const std::filesystem::path& filepath, const GlobalAppSettings& current_settings = GlobalAppSettings());
	GlobalAppSettings parse(const std::filesystem::path& config_filepath);
	std::filesystem::path get_app_config_path();
	GlobalAppSettings get_app_config();
}
