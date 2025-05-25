#pragma once

#include <string>
#include <functional>

namespace desktop_notification {
	using ClickCallback = std::function<void()>;

	// Initialise notification system and request permissions
	bool initialise(const std::string& app_name);

	// Show a notification with optional click callback
	bool show(const std::string& title, const std::string& message, ClickCallback on_click = nullptr);

	bool is_supported();
	bool has_permission();
	void cleanup();
}
