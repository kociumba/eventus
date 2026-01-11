#include <eventus>
#include <print>

bool sub_func(const char** data) {
    std::println("  Free function subscriber: '{}'", *data);
    return true;
}

int main(int argc, char** argv) {
    auto b = eventus::bus();

    // Lambda subscriber (explicit type or auto works)
    eventus::subscribe<const char*>(&b, [&](auto* data) {
        std::println("  Lambda subscriber: '{}'", *data);
        return true;
    });

    // Function pointer subscriber
    eventus::subscribe<const char*>(&b, sub_func);

    std::println("Publishing 'gabagool':");
    eventus::publish(&b, "gabagool");

    std::println("\nPublishing 'something creative':");
    eventus::publish(&b, "something creative");

    return 0;
}