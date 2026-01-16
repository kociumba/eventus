#include <eventus>
#include <print>

// Subscribers receive a pointer to the event data
bool sub_func(const char** data) {
    std::println("  Free function subscriber: '{}'", *data);
    return true;
}

int main() {
    std::println("=== {} ===\n", __FILE__);

    auto b = eventus::bus();

    // --- Scenario 1: Subscribing with Lambdas ---
    // We use types as events, here 'const char*'
    eventus::subscribe<const char*>(&b, [](auto* data) {
        std::println("  Lambda subscriber: '{}'", *data);
        return true;
    });

    // --- Scenario 2: Subscribing with Function Pointers ---
    eventus::subscribe<const char*>(&b, sub_func);

    // --- Scenario 3: Subscribing to a single instance of an event ---
    eventus::once<const char*>(&b, [](auto* data) {
        std::println("  Once subscriber: '{}'", *data);
        return true;
    });

    // --- Scenario 4: Publishing Events ---
    std::println("=== Initial State: Three subscribers registered (one is once-only) ===");

    std::println("Publishing 'gabagool':");
    eventus::publish(&b, "gabagool");

    std::println("\nPublishing 'something creative':");
    eventus::publish(&b, "something creative");

    std::println("\n=== Summary ===");
    std::println("eventus::bus: The central communication hub for your application");
    std::println("eventus::subscribe: Registers a callback for a specific type T");
    std::println(
        "eventus::once: Registers a one-time callback that auto-unsubscribes after the first "
        "recieved event");
    std::println("eventus::publish: Distributes data to all listeners of that type");
    std::println(
        "Functional Style: Always passes &bus as the first argument (see 02_bus_methods for "
        "other style)");
    std::println("\n");
    return 0;
}