#include <eventus>
#include <print>

struct CleanupEvent {
    int value;
};

struct AnotherEvent {
    std::string text;
};

int main() {
    std::println("=== {} ===\n", __FILE__);

    auto b = eventus::bus();

    int64_t subs[3];
    int id1 = 0;
    int id2 = 1;
    int id3 = 2;

    // Subscribe multiple handlers to CleanupEvent
    subs[id1] = eventus::subscribe<CleanupEvent>(&b, [&](CleanupEvent* e) {
        std::println("  Subscriber 1 (ID: {}): value = {}", subs[id1], e->value);
        return true;
    });

    subs[id2] = eventus::subscribe<CleanupEvent>(&b, [&](CleanupEvent* e) {
        std::println("  Subscriber 2 (ID: {}): value = {}", subs[id2], e->value);
        return true;
    });

    subs[id3] = eventus::subscribe<CleanupEvent>(&b, [&](CleanupEvent* e) {
        std::println("  Subscriber 3 (ID: {}): value = {}", subs[id3], e->value);
        return true;
    });

    // Subscribe to a different event type
    eventus::subscribe<AnotherEvent>(&b, [](AnotherEvent* e) {
        std::println("  AnotherEvent subscriber: text = '{}'", e->text);
        return true;
    });

    std::println("=== Initial State: All subscribers active ===");
    std::println("Subscriber IDs: {}, {}, {}\n", subs[id1], subs[id2], subs[id3]);
    eventus::publish(&b, CleanupEvent{420});
    eventus::publish(&b, AnotherEvent{"Still here"});

    // Unsubscribe one by ID
    std::println("\n=== Unsubscribe Subscriber 2 (ID: {}) ===", subs[id2]);
    auto status = eventus::unsubscribe<CleanupEvent>(&b, subs[id2]);
    std::println("Status: {}\n", eventus::status_string(status));
    eventus::publish(&b, CleanupEvent{69});

    // Unsubscribe another by ID without event type
    std::println("\n=== Unsubscribe Subscriber 1 (ID: {}) ===", subs[id1]);
    status = eventus::unsubscribe(&b, subs[id1]);
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
    std::println("unsubscribe_all: clears all subscribers and events in the bus\n\n");

    return 0;
}
