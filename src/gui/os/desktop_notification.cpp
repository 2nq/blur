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
#	include <libnotify/notify.h>

static std::string g_app_name;
static desktop_notification::ClickCallback g_click_callback;
static bool g_is_initialised = false;

static void on_notification_closed(NotifyNotification* notification, gpointer user_data) {
	g_object_unref(G_OBJECT(notification));
}

static void on_notification_activated(NotifyNotification* notification, char* action, gpointer user_data) {
	if (g_click_callback) {
		g_click_callback();
	}
}

bool desktop_notification::initialise(const std::string& app_name) {
	if (g_is_initialised) {
		return true;
	}

	g_app_name = app_name;
	g_is_initialised = notify_init(app_name.c_str());
	return g_is_initialised;
}

bool desktop_notification::show(const std::string& title, const std::string& message, ClickCallback on_click) {
	if (!g_is_initialised) {
		if (!initialise(g_app_name.empty() ? "App" : g_app_name)) {
			return false;
		}
	}

	if (!notify_is_initted())
		return false;

	g_click_callback = on_click;

	NotifyNotification* notification = notify_notification_new(title.c_str(), message.c_str(), nullptr);
	if (!notification)
		return false;

	notify_notification_set_timeout(notification, 5000); // 5 seconds

	if (on_click) {
		notify_notification_add_action(
			notification, "default", "Open", NOTIFY_ACTION_CALLBACK(on_notification_activated), nullptr, nullptr
		);
	}

	g_signal_connect(notification, "closed", G_CALLBACK(on_notification_closed), nullptr);

	GError* error = nullptr;
	bool success = notify_notification_show(notification, &error);

	if (error) {
		g_error_free(error);
		g_object_unref(G_OBJECT(notification));
		return false;
	}

	return success;
}

bool desktop_notification::is_supported() {
	return notify_is_initted();
}

bool desktop_notification::has_permission() {
	return notify_is_initted();
}

void desktop_notification::cleanup() {
	if (!g_is_initialised) {
		return;
	}
	if (notify_is_initted()) {
		notify_uninit();
	}
	g_is_initialised = false;
}

#endif
