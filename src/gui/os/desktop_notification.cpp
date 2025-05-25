#include "desktop_notification.h"

#ifdef _WIN32
#	include "common/blur.h"
#	include "sdl.h"
#	include <shellapi.h>
#	include <winrt/Windows.Foundation.h>
#	include <winrt/Windows.UI.Notifications.h>
#	include <winrt/Windows.Data.Xml.Dom.h>
#	include <winrt/Windows.ApplicationModel.h>
using namespace winrt;
using namespace Windows::UI::Notifications;
using namespace Windows::Data::Xml::Dom;

static std::string g_app_name;
static desktop_notification::ClickCallback g_click_callback;
static bool g_initialised = false;

bool desktop_notification::initialise(const std::string& app_name) {
	if (g_initialised)
		return true;

	// winrt::init_apartment(); // don't need it?

	g_app_name = app_name;
	g_initialised = true;
	return true;
}

bool desktop_notification::show(const std::string& title, const std::string& message, ClickCallback on_click) {
	// don't show notification if window is focused
	if (SDL_GetWindowFlags(sdl::window) & SDL_WINDOW_INPUT_FOCUS)
		return false;

	if (!g_initialised)
		initialise(APPLICATION_NAME);

	g_click_callback = on_click;

	try {
		auto toast_xml = ToastNotificationManager::GetTemplateContent(ToastTemplateType::ToastText02);

		auto text_elements = toast_xml.GetElementsByTagName(L"text");
		text_elements.Item(0).AppendChild(toast_xml.CreateTextNode(winrt::to_hstring(title)));
		text_elements.Item(1).AppendChild(toast_xml.CreateTextNode(winrt::to_hstring(message)));

		// // ToastImageAndText02
		// auto image_elements = toast_xml.GetElementsByTagName(L"image");
		// auto image_element = image_elements.Item(0).as<Windows::Data::Xml::Dom::XmlElement>();
		// image_element.SetAttribute(
		// 	L"src", winrt::hstring(L"file:///[something]/blur.ico")
		// );
		// image_element.SetAttribute(L"alt", winrt::hstring(L"Blur icon"));

		auto toast = ToastNotification(toast_xml);

		if (on_click) {
			toast.Activated([](ToastNotification const&, auto const&) {
				SDL_RaiseWindow(sdl::window); // probably won't work.

				if (g_click_callback)
					g_click_callback();
			});
		}

		ToastNotificationManager::CreateToastNotifier(winrt::to_hstring(g_app_name)).Show(toast);
		return true;
	}
	catch (...) {
		return false;
	}
}

bool desktop_notification::is_supported() {
	return true;
}

bool desktop_notification::has_permission() {
	return true;
}

void desktop_notification::cleanup() {
	if (!g_initialised)
		return;

	g_initialised = false;
}

#elif __linux__
#	include <sdbus-c++/sdbus-c++.h>
#	include <iostream>
#	include <unordered_map>
#	include <mutex>
#	include <thread>

namespace desktop_notification {
	// Simple globals for state
	static std::unique_ptr<sdbus::IConnection> g_connection;
	static std::unique_ptr<sdbus::IProxy> g_proxy;
	static std::string g_app_name;
	static bool g_initialized = false;
	static std::unordered_map<uint32_t, ClickCallback> g_callbacks;
	static std::mutex g_callbacks_mutex;
	static std::thread g_event_thread;
	static bool g_running = false;

	static constexpr const char* SERVICE = "org.freedesktop.Notifications";
	static constexpr const char* PATH = "/org/freedesktop/Notifications";
	static constexpr const char* INTERFACE = "org.freedesktop.Notifications";

	static void handle_action_invoked(sdbus::Signal& signal) {
		uint32_t id;
		std::string action;
		signal >> id >> action;

		if (action == "default") {
			std::lock_guard<std::mutex> lock(g_callbacks_mutex);
			auto it = g_callbacks.find(id);
			if (it != g_callbacks.end()) {
				// Run callback in detached thread
				std::thread([cb = it->second]() {
					cb();
				}).detach();
			}
		}
	}

	static void handle_notification_closed(sdbus::Signal& signal) {
		uint32_t id;
		uint32_t reason;
		signal >> id >> reason;

		std::lock_guard<std::mutex> lock(g_callbacks_mutex);
		g_callbacks.erase(id);
	}

	static void event_loop() {
		try {
			while (g_running) {
				g_connection->processPendingEvent();
			}
		}
		catch (const std::exception& e) {
			std::cerr << "Event loop error: " << e.what() << std::endl;
		}
	}

	bool initialise(const std::string& app_name) {
		if (g_initialized) {
			return true;
		}

		try {
			g_app_name = app_name;

			// Create connection and proxy
			g_connection = sdbus::createSessionBusConnection();
			g_proxy = sdbus::createProxy(*g_connection, SERVICE, PATH);

			// Register signal handlers
			g_proxy->registerSignalHandler(INTERFACE, "ActionInvoked", handle_action_invoked);
			g_proxy->registerSignalHandler(INTERFACE, "NotificationClosed", handle_notification_closed);
			g_proxy->finishRegistration();

			// Start event loop
			g_running = true;
			g_event_thread = std::thread(event_loop);

			g_initialized = true;
			return true;
		}
		catch (const std::exception& e) {
			std::cerr << "Failed to initialize: " << e.what() << std::endl;
			return false;
		}
	}

	bool show(const std::string& title, const std::string& message, ClickCallback on_click) {
		if (!g_initialized) {
			if (!initialise(APPLICATION_NAME)) {
				std::cerr << "Not initialized" << std::endl;
				return false;
			}
		}

		try {
			std::vector<std::string> actions;
			if (on_click) {
				actions = { "default", "Click" };
			}

			auto method = g_proxy->createMethodCall(INTERFACE, "Notify");
			method << g_app_name                              // app_name
				   << uint32_t(0)                             // replaces_id
				   << std::string("")                         // app_icon
				   << title                                   // summary
				   << message                                 // body
				   << actions                                 // actions
				   << std::map<std::string, sdbus::Variant>{} // hints
				   << int32_t(-1);                            // timeout

			auto reply = g_proxy->callMethod(method);
			uint32_t id;
			reply >> id;

			if (on_click) {
				std::lock_guard<std::mutex> lock(g_callbacks_mutex);
				g_callbacks[id] = on_click;
			}

			return true;
		}
		catch (const std::exception& e) {
			std::cerr << "Failed to show notification: " << e.what() << std::endl;
			return false;
		}
	}

	bool is_supported() {
		try {
			auto conn = sdbus::createSessionBusConnection();
			auto proxy = sdbus::createProxy(*conn, SERVICE, PATH);
			auto method = proxy->createMethodCall(INTERFACE, "GetCapabilities");
			proxy->callMethod(method);
			return true;
		}
		catch (...) {
			return false;
		}
	}

	bool has_permission() {
		return is_supported(); // On Linux, D-Bus access implies permission
	}

	void cleanup() {
		if (!g_initialized)
			return;

		g_running = false;

		if (g_event_thread.joinable()) {
			try {
				g_connection->leaveEventLoop();
			}
			catch (...) {
			}
			g_event_thread.join();
		}

		{
			std::lock_guard<std::mutex> lock(g_callbacks_mutex);
			g_callbacks.clear();
		}

		g_proxy.reset();
		g_connection.reset();
		g_initialized = false;
	}
}
#endif
