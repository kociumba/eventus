# eventus

Eventus is a minimalist, user experience first event system in c++20/23

What this corpo speak means is that eventus is a single header library, just copy [`eventus.h`](eventus.h) into your project.

It is cross-platform and only requires a c++20/23 compatible compiler and the STL, any documentation needed can be found inside the file in code comments or by looking at [/examples](/examples) (work in progress), examples are numbered in order of simplest to most complicated, and it is advised you explore them in that order.

To run any of the examples yourself:
1. Clone the repo
2. Run: `xmake r [exmaple_filename]` (`xmake r 04_propagation`)

Quick example:

```c++
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