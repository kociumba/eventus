#include <eventus>
#include <print>

int main(int argc, char** argv) {
    auto b = eventus::bus();

    eventus::subscribe<const char*>(&b, [&](const char** data) {
        std::println("data: {}", *data);
        return true;
    });

    eventus::publish(&b, "gabagool");
    eventus::publish(&b, "something creative");

    return 0;
}
