#include <eventus.h>
#include <chrono>
#include <print>
#include <thread>

struct message {
    std::string content;
    int id;
};

int main(int argc, char** argv) {
    // if using threading you can use a custom sized thread pool by initializing like so bus(thread_pool_size)
    auto b = eventus::bus();

    // Subscribe two handlers with different priorities
    eventus::subscribe<message>(
        &b,
        [](message* msg) {
            std::println("Handler A received '{}' (id: {}) on thread: {}",
                         msg->content,
                         msg->id,
                         std::this_thread::get_id());
            _sleep(50);
            return true;
        },
        5);

    eventus::subscribe<message>(
        &b,
        [](message* msg) {
            std::println("Handler B received '{}' (id: {}) on thread: {}",
                         msg->content,
                         msg->id,
                         std::this_thread::get_id());
            _sleep(50);
            return true;
        },
        10);

    std::println("Main thread: {}\n", std::this_thread::get_id());

    // publish_threaded: Both handlers execute sequentially on one worker thread
    std::println("=== publish_threaded (sequential on worker thread) ===");
    eventus::publish_threaded(&b, message{"First message", 1});
    _sleep(150);

    // publish_async: Each subscriber executes on a different worker thread
    std::println("\n=== publish_async (parallel on separate threads) ===");
    eventus::publish_async(&b, message{"Second message", 2});
    _sleep(150);

    // publish_threaded_multi: Each event gets its own worker thread,
    // but subscribers for each event still execute sequentially
    std::println("\n=== publish_threaded_multi (multiple events, parallel) ===");
    eventus::publish_threaded_multi(&b, message{"Third message", 3}, message{"Fourth message", 4});
    _sleep(200);

    return 0;
}