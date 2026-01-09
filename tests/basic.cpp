// #define EVENTUS_THREAD_SAFE
// #define EVENTUS_NO_THREADING
#define EVENTUS_BUS_METHODS
#include <eventus>
#include <print>

int main(int argc, char** argv) {
    auto b = eventus::bus();

    eventus::subscribe<const char*>(&b, [&](const char** data) {
        std::println("data: {}", *data);
        return true;
    });

    eventus::publish_async(&b, "gabagool");
    eventus::publish_async(&b, "something creative");

    // while (true) {
    _sleep(100);
    // }

    return 0;
}
