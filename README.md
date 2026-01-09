# eventus

Eventus is a minimalist, user experience first event system in c++20/23

What this corpo speak means is that eventus is a single header library, just copy `eventus.h` into your project.

It is cross-platform and only requires a c++20/23 compatible compiler and the STL, any documentation needed can be found inside the file in code comments or by looking at [/examples](/examples) (work in progress)

Quick example:

```c++
#define EVENTUS_THREAD_SAFE
#define EVENTUS_BUS_METHODS
#include <eventus>
#include <print>

int main(int argc, char** argv) {
    auto b = eventus::bus();

    eventus::subscribe<const char*>(&b, [&](const char** data) {
        std::println("data: {}", *data);
        return true; // report no errors
    });

    eventus::publish(&b, "gabagool");
    eventus::publish(&b, "something creative");
}
```