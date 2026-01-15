#define EVENTUS_DEBUG_LOG
#include <eventus>
#include <iostream>
#include <print>

struct user_action {
    std::string action;
};

struct unregistered_event {
    int x;
};

int main() {
    std::println("=== {} ===\n", __FILE__);

    auto b = eventus::bus();

    // The default logger is already set, but we can customize it
    std::println("=== Scenario 1: Default Library Logger ===");

    auto id1 = eventus::subscribe<user_action>(&b, [](user_action* e) {
        std::println("  [Handler] User action: {}", e->action);
        return true;
    });

    eventus::publish(&b, user_action{"clicked button"});
    id1.unsubscribe();

    std::println("\n=== Scenario 2: Setting a Custom Logger ===");

    eventus::set_logger(&b, [](eventus::ev_log_data data) {
        std::string_view level_str;
        switch (data.level) {
            case eventus::DEBUG:
                level_str = "DEBUG";
                break;
            case eventus::INFO:
                level_str = "INFO";
                break;
            case eventus::WARNING:
                level_str = "WARN";
                break;
            case eventus::ERROR:
                level_str = "ERROR";
                break;
            case eventus::FATAL:
                level_str = "FATAL";
                break;
            default:
                level_str = "LOG";
                break;
        }

        std::println("[CUSTOM] {:<5} | {}", level_str, data.format());
    });

    // This subscription will now be logged via our [CUSTOM] logger
    auto id2 = eventus::subscribe<user_action>(&b, [](user_action* e) {
        std::println("  [Handler] User action: {}", e->action);
        return true;
    });

    eventus::publish(&b, user_action{"submitted form"});

    std::println("\n=== Scenario 3: Debugging Unregistered Events ===");

    // Attempting to publish an event that has no subscribers.
    // The logger will report the unregisted event.
    eventus::publish(&b, unregistered_event{42});

    std::println("\n=== Summary ===");
    std::println(
        "EVENTUS_DEBUG_LOG: Must be defined before including <eventus> for logging functionality "
        "to be present");
    std::println("ev_log_data: Contains level, captured data and the log message");
    std::println(
        "set_logger: Allows providing your own logger, for custom logging, or logger sytem "
        "integration");
    std::println("ev_log_data.format(): formats log messages with the captured data for display\n");

    return 0;
}