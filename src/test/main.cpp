#include <iostream>
#include <libnotify/notify.h>

int main(int argc, char* argv[]) {
	if (!notify_init("Sample")) {
		std::cerr << "failed to initialise notify" << '\n';
		return -1;
	}

	NotifyNotification* n = notify_notification_new("test notification", "description", 0);
	notify_notification_set_timeout(n, 10000); // 10 seconds

	GError* error = nullptr;
	if (!notify_notification_show(n, &error)) {
		std::cerr << "show has failed " << (error ? error->message : "unknown error") << '\n';

		if (error)
			g_error_free(error);

		notify_uninit();
        
		return -1;
	}

	notify_uninit();

	return 0;
}
