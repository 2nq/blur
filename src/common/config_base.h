#pragma once

namespace config_base {
	template<typename T>
	void extract_toml_value(const toml::table& config, const std::string& key, T& out, const T& default_value) {
		try {
			auto node = config.at_path(key);
			if (node) {
				if constexpr (std::is_same_v<T, std::string>) {
					if (auto val = node.value<std::string>()) {
						out = *val;
					}
					else {
						out = default_value;
					}
				}
				else if constexpr (std::is_same_v<T, bool>) {
					if (auto val = node.value<bool>()) {
						out = *val;
					}
					else {
						out = default_value;
					}
				}
				else if constexpr (std::is_same_v<T, int>) {
					if (auto val = node.value<int64_t>()) {
						out = static_cast<int>(*val);
					}
					else {
						out = default_value;
					}
				}
				else if constexpr (std::is_same_v<T, float>) {
					if (auto val = node.value<double>()) {
						out = static_cast<float>(*val);
					}
					else {
						out = default_value;
					}
				}
				else {
					// For other types, try direct assignment
					if (auto val = node.value<T>()) {
						out = *val;
					}
					else {
						out = default_value;
					}
				}
			}
			else {
				DEBUG_LOG("TOML config missing key '{}'", key);
				out = default_value;
			}
		}
		catch (const std::exception& e) {
			DEBUG_LOG("Failed to parse TOML config key '{}': {}", key, e.what());
			out = default_value;
		}
	}

	template<typename ConfigType>
	ConfigType load_config(
		const std::filesystem::path& config_path,
		void (*create_func)(const std::filesystem::path&, const ConfigType&),
		ConfigType (*parse_func)(const std::filesystem::path&)
	) {
		bool config_exists = std::filesystem::exists(config_path);

		if (!config_exists) {
			create_func(config_path, ConfigType());

			if (blur.verbose)
				u::log(L"Configuration file not found, default config generated at {}", config_path.wstring());
		}

		return parse_func(config_path);
	}
}
