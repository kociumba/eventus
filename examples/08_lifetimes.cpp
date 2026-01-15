#include <eventus>
#include <print>

struct event {
    std::string message;
};

int main() {
    std::println("=== {} ===\n", __FILE__);

    auto b = eventus::bus();

    std::println("=== Scenario 1: Automatic RAII Cleanup (Scope-based) ===");
    {
        // Using scoped() to tie lifetime to the '{ }' block
        eventus::owned_id id = eventus::subscribe<event>(&b, [](event* n) {
            std::println("  Callback: Received '{}'", n->message);
            return true;
        }).scoped();

        std::println("Subscription active. owned_id is valid: {}", id.valid() ? "yes" : "no");
        eventus::publish(&b, event{"Message inside scope"});

        std::println("Leaving scope... (Destructor will call unsubscribe)");
    }

    std::println("\nOutside scope. Publishing again:");
    auto status = eventus::publish(&b, event{"Message outside scope"});
    std::println("Publish status: {}", eventus::status_string(status));

    std::println("\n=== Scenario 2: Pipe Operator & Manual Release ===");
    eventus::ev_id regular_id;  // by default ev_id is invalid and will fail any operations

    {
        // Using the pipe operator style
        auto owned = eventus::subscribe<event>(&b, [](event* n) {
            std::println("  Callback: Received '{}'", n->message);
            return true;
        }) | eventus::scoped;

        std::println("Subscription active via pipe operator.");
        eventus::publish(&b, event{"Message before release"});

        // Transitioning from RAII back to manual management
        std::println("\nReleasing ownership to a regular ID...");
        regular_id = owned.release();

        std::println("owned_id is now valid: {}", owned.valid() ? "yes" : "no");
        std::println("Leaving scope... (Subscription should persist)");
    }

    std::println("\nOutside scope (after release):");
    status = eventus::publish(&b, event{"Message after release"});
    std::println("Publish status: {}", eventus::status_string(status));

    // Manual cleanup for the released ID
    auto unsub_status = regular_id.unsubscribe();
    std::println("Unsubscribe status: {}", eventus::status_string(unsub_status));

    std::println("\n=== Summary ===");
    std::println("owned_id: Unsubscribes automatically when it is destroyed");
    std::println(".scoped(): Converts a regular ID to an owned ID");
    std::println(".release(): Transfers responsibility back to the user (stops RAII)");
    std::println(
        "This is ideal for managing subscriber lifetimes tied to UI components or objects.\n");

    return 0;
}