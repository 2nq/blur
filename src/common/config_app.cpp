#include "config_app.h"
#include "config_base.h"
#include <toml++/toml.hpp>

void config_app::create(const std::filesystem::path& filepath, const GlobalAppSettings& current_settings) {
	toml::table config;

	// Version
	config.insert("version", "v" + BLUR_VERSION);

	// Notifications section
	auto notifications_table = toml::table{};
	notifications_table.insert("render_success_notifications", current_settings.render_success_notifications);
	notifications_table.insert("render_failure_notifications", current_settings.render_failure_notifications);
	config.insert("notifications", notifications_table);

	// Updates section
	auto updates_table = toml::table{};
	updates_table.insert("check_for_updates", current_settings.check_updates);
	updates_table.insert("include_beta_updates", current_settings.check_beta);
	config.insert("updates", updates_table);

#ifdef __linux__
	// Linux section
	auto linux_table = toml::table{};
	linux_table.insert("vapoursynth_lib_path", current_settings.vapoursynth_lib_path);
	config.insert("linux", linux_table);
#endif

	// Write to file
	std::ofstream output(filepath);
	output << config;
}

GlobalAppSettings config_app::parse(const std::filesystem::path& config_filepath) {
	GlobalAppSettings settings;

	try {
		toml::table config = toml::parse_file(config_filepath.string());

		// Extract values using the template functions from config_base.h
		config_base::extract_toml_value(
			config,
			"notifications.render_success_notifications",
			settings.render_success_notifications,
			DEFAULT_APP_CONFIG.render_success_notifications
		);
		config_base::extract_toml_value(
			config,
			"notifications.render_failure_notifications",
			settings.render_failure_notifications,
			DEFAULT_APP_CONFIG.render_failure_notifications
		);

		config_base::extract_toml_value(
			config, "updates.check_for_updates", settings.check_updates, DEFAULT_APP_CONFIG.check_updates
		);
		config_base::extract_toml_value(
			config, "updates.include_beta_updates", settings.check_beta, DEFAULT_APP_CONFIG.check_beta
		);

#ifdef __linux__
		config_base::extract_toml_value(
			config, "linux.vapoursynth_lib_path", settings.vapoursynth_lib_path, DEFAULT_APP_CONFIG.vapoursynth_lib_path
		);
#endif
	}
	catch (const toml::parse_error& err) {
		DEBUG_LOG("Error parsing TOML config file at %s: %s", config_filepath.string().c_str(), err.what());
		return GlobalAppSettings(); // Return default settings on parse error
	}

	// Recreate the config file using the parsed values (keeps nice formatting)
	create(config_filepath, settings);

	return settings;
}

std::filesystem::path config_app::get_app_config_path() {
	return blur.settings_path / APP_CONFIG_FILENAME;
}

GlobalAppSettings config_app::get_app_config() {
	return config_base::load_config<GlobalAppSettings>(get_app_config_path(), create, parse);
}
