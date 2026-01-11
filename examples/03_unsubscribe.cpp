#include <eventus>
#include <print>

struct CleanupEvent {
    int value;
};

struct AnotherEvent {
    std::string text;
};

int main() {
    auto b = eventus::bus();

    // Subscribe multiple handlers to CleanupEvent
    auto id1 = eventus::subscribe<CleanupEvent>(&b, [](CleanupEvent* e) {
        std::println("  Subscriber 1 (ID: ?): value = {}", e->value);
        return true;
    });

    auto id2 = eventus::subscribe<CleanupEvent>(&b, [](CleanupEvent* e) {
        std::println("  Subscriber 2 (ID: ?): value = {}", e->value);
        return true;
    });

    auto id3 = eventus::subscribe<CleanupEvent>(&b, [](CleanupEvent* e) {
        std::println("  Subscriber 3 (ID: ?): value = {}", e->value);
        return true;
    });

    // Subscribe to a different event type
    eventus::subscribe<AnotherEvent>(&b, [](AnotherEvent* e) {
        std::println("  AnotherEvent subscriber: text = '{}'", e->text);
        return true;
    });

    std::println("=== Initial State: All subscribers active ===");
    std::println("Subscriber IDs: {}, {}, {}\n", id1, id2, id3);
    eventus::publish(&b, CleanupEvent{420});
    eventus::publish(&b, AnotherEvent{"Still here"});

    // Unsubscribe one by ID
    std::println("\n=== Unsubscribe Subscriber 2 (ID: {}) ===", id2);
    auto status = eventus::unsubscribe<CleanupEvent>(&b, id2);
    std::println("Status: {}\n", eventus::status_string(status));
    eventus::publish(&b, CleanupEvent{69});

    // Unsubscribe another by ID
    std::println("\n=== Unsubscribe Subscriber 1 (ID: {}) ===", id1);
    status = eventus::unsubscribe<CleanupEvent>(&b, id1);
    std::println("Status: {}\n", eventus::status_string(status));
    eventus::publish(&b, CleanupEvent{2137});

    // Unsubscribe entire event type (removes remaining subscriber 3)
    std::println("\n=== Unsubscribe entire CleanupEvent type ===");
    status = eventus::unsubscribe_event<CleanupEvent>(&b);
    std::println("Status: {}\n", eventus::status_string(status));

    std::println("Publishing CleanupEvent (no subscribers remain):");
    status = eventus::publish(&b, CleanupEvent{1337});
    std::println("Status: {}", eventus::status_string(status));

    std::println("\nPublishing AnotherEvent (still has subscribers):");
    eventus::publish(&b, AnotherEvent{"Still working"});

    std::println("\n=== Summary ===");
    std::println("unsubscribe<T>: Removes specific subscriber by ID");
    std::println("unsubscribe_event<T>: Removes all subscribers for event type");
    std::println("Other event types remain unaffected");
    std::println("unsubscribe_all: clears all subscribers and events in the bus");

    return 0;
}
