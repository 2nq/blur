#include <sdbus-c++/sdbus-c++.h>
#include <iostream>
#include <vector>
#include <map>

int main() {
    try {
        // Create connection to session bus
        auto connection = sdbus::createSessionBusConnection();
        
        // Create proxy object for the notification service
        auto proxy = sdbus::createProxy(*connection, 
                                       sdbus::ServiceName{"org.freedesktop.Notifications"}, 
                                       sdbus::ObjectPath{"/org/freedesktop/Notifications"});
        
        // Prepare notification parameters
        std::string app_name = "MyApp";
        uint32_t replaces_id = 0;  // 0 means new notification
        std::string app_icon = "";  // Empty string for default icon
        std::string summary = "Hello World!";
        std::string body = "This is a test notification from sdbus-c++";
        std::vector<std::string> actions;  // No actions
        std::map<std::string, sdbus::Variant> hints;  // No hints
        int32_t expire_timeout = 5000;  // 5 seconds
        
        // Call the Notify method
        uint32_t notification_id;
        proxy->callMethod("Notify")
             .onInterface("org.freedesktop.Notifications")
             .withArguments(app_name, 
                          replaces_id, 
                          app_icon, 
                          summary, 
                          body, 
                          actions, 
                          hints, 
                          expire_timeout)
             .storeResultsTo(notification_id);
        
        std::cout << "Notification sent! ID: " << notification_id << std::endl;
        
    } catch (const sdbus::Error& e) {
        std::cerr << "D-Bus error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}