#define EVENTUS_BUS_METHODS
#include <eventus>
#include <print>

bool sub_func(const char** data) {
    std::println("  Free function subscriber: '{}'", *data);
    return true;
}

// This is the same example as 01_basic, but rewritten to use the bus methods
int main() {
    std::println("=== {} ===\n", __FILE__);

    // The bus object has member functions defined due to #define EVENTUS_BUS_METHODS
    auto b = eventus::bus();

    std::println("=== Using Member Function Syntax ===");

    // b.subscribe instead of eventus::subscribe(&b, ...)
    b.subscribe<const char*>([](auto* data) {
        std::println("  Lambda subscriber: '{}'", *data);
        return true;
    });

    // Subscribing a free function via member method
    b.subscribe<const char*>(sub_func);

    // Subscribing to a single instance of an event
    eventus::once<const char*>(&b, [](auto* data) {
        std::println("  Once subscriber: '{}'", *data);
        return true;
    });

    // b.publish instead of eventus::publish(&b, ...)
    std::println("Publishing 'gabagool':");
    b.publish("gabagool");

    std::println("\nPublishing 'something creative':");
    b.publish("something creative");

    std::println("\n=== Summary ===");
    std::println(
        "EVENTUS_BUS_METHODS: Must be defined before including <eventus> for bus methods to be "
        "included");
    std::println(
        "Member Syntax: b.publish(data) is shorthand for eventus::publish(&b, data) and so on for "
        "all methods");
    std::println(
        "Paradigm: This makes eventus more oop-ish, for a more functional style see 01_basic");
    std::println("\n");

    return 0;
}