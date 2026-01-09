#define EVENTUS_THREAD_SAFE
#define EVENTUS_BUS_METHODS
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
}
