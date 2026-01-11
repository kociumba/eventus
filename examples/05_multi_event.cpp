#include <eventus>
#include <print>

struct EventA {
    std::string info;
};

struct EventB {
    int number;
};

struct EventC {
    bool flag;
};

int main() {
    auto b = eventus::bus();

    // Multi-subscribe: ONE handler for MULTIPLE event types
    std::println("=== Setting up multi-subscriber ===");
    auto ids = eventus::subscribe_multi<EventA, EventB>(&b, [](auto* e) {
        if constexpr (std::is_same_v<decltype(e), EventA*>) {
            std::println("  [Multi-handler] EventA: info = '{}'", e->info);
        } else if constexpr (std::is_same_v<decltype(e), EventB*>) {
            std::println("  [Multi-handler] EventB: number = {}", e->number);
        }
        return true;
    });

    std::println(
        "Multi-handler registered for EventA (ID: {}) and EventB (ID: {})\n", ids[0], ids[1]);

    // Additional specific subscribers
    eventus::subscribe<EventA>(&b, [](EventA* e) {
        std::println("  [EventA-only handler] Received: '{}'", e->info);
        return true;
    });

    eventus::subscribe<EventB>(&b, [](EventB* e) {
        std::println("  [EventB-only handler] Received: {}", e->number);
        return true;
    });

    eventus::subscribe<EventC>(&b, [](EventC* e) {
        std::println("  [EventC handler] Flag = {}", e->flag);
        return true;
    });

    // Individual publishes - show each event triggers its handlers
    std::println("=== Individual Publishes ===");
    std::println("\nPublishing EventA:");
    eventus::publish(&b, EventA{"Hello"});

    std::println("\nPublishing EventB:");
    eventus::publish(&b, EventB{420});

    std::println("\nPublishing EventC:");
    eventus::publish(&b, EventC{true});

    // Multi-publish: Publish MULTIPLE events at once
    std::println("\n=== Multi-Publish (A, B, C simultaneously) ===");
    eventus::publish_multi(&b, EventA{"World"}, EventB{69}, EventC{false});

    // Demonstrate unsubscribing from multi-subscriber
    std::println("\n=== Unsubscribe multi-handler from EventA only ===");
    eventus::unsubscribe<EventA>(&b, ids[0]);

    std::println("\nPublishing EventA (multi-handler removed, specific handler remains):");
    eventus::publish(&b, EventA{"After unsubscribe"});

    std::println("\nPublishing EventB (multi-handler still active):");
    eventus::publish(&b, EventB{1337});

    std::println("\n=== Summary ===");
    std::println("subscribe_multi: One handler for multiple event types");
    std::println("publish_multi: Publish multiple events in one call");
    std::println("Multi-subscribers can be unsubscribed per event type");
    std::println("Each event type maintains its own subscriber list");

    return 0;
}
