#include <eventus>
#include <print>
#include "common.h"

struct message {
    std::string content;
    int id;
};

int main() {
    std::println("=== {} ===\n", __FILE__);

    // Custom thread pool size: bus(thread_pool_size)
    auto b = eventus::bus();

    // Register handlers in mixed priority order to show they execute by priority

    // Registered FIRST, but lower priority (5)
    eventus::subscribe<message>(
        &b,
        [](message* msg) {
            std::println("  [Priority 5] Handler A: '{}' (id: {}) on thread: {}",
                msg->content,
                msg->id,
                std::this_thread::get_id());
            ev_sleep(50);
            return true;
        },
        5);

    // Registered SECOND, but higher priority (10) - executes first
    eventus::subscribe<message>(
        &b,
        [](message* msg) {
            std::println("  [Priority 10] Handler B: '{}' (id: {}) on thread: {}",
                msg->content,
                msg->id,
                std::this_thread::get_id());
            ev_sleep(50);
            return true;
        },
        10);

    std::println("Main thread: {}\n", std::this_thread::get_id());

    // publish_threaded: Both handlers execute SEQUENTIALLY on ONE worker thread
    // Handlers run in priority order (B then A) on the same worker thread
    std::println("=== publish_threaded: Sequential on single worker thread ===");
    std::println("Expected: Both handlers on same worker thread, B before A\n");
    eventus::publish_threaded(&b, message{"First message", 69});
    ev_sleep(150);

    // publish_async: Each handler executes on its OWN worker thread IN PARALLEL
    // Handlers still respect priority but run simultaneously
    std::println("\n=== publish_async: Parallel execution on separate threads ===");
    std::println("Expected: Handlers on different threads, may interleave output\n");
    eventus::publish_async(&b, message{"Second message", 420});
    ev_sleep(150);

    // publish_threaded_multi: Each EVENT gets its own worker thread
    // Handlers for each event still execute sequentially, but events run in parallel
    std::println("\n=== publish_threaded_multi: Multiple events in parallel ===");
    std::println("Expected: Two events on different threads, each event's handlers sequential\n");
    eventus::publish_threaded_multi(
        &b, message{"Third message", 2137}, message{"Fourth message", 1337});
    ev_sleep(200);

    std::println("\n=== Summary ===");
    std::println("publish_threaded: One worker thread, handlers sequential");
    std::println("publish_async: Multiple worker threads, handlers parallel");
    std::println("publish_threaded_multi: One worker per event, handlers sequential per event\n\n");

    return 0;
}
