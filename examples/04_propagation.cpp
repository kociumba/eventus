#include <eventus>
#include <print>

struct ModifiableEvent {
    int value = 0;
    std::string status = "Initial";
};

int main() {
    auto b = eventus::bus();

    // Register subscribers in MIXED order to demonstrate that priority overrides registration order

    // Registered FIRST, but runs FOURTH (priority -10)
    eventus::subscribe<ModifiableEvent>(
        &b,
        [](ModifiableEvent* e) {
            e->value += 5;
            e->status += " -> Low Priority Added 5";
            std::println("  [Priority -10] Low: value={}, status='{}'", e->value, e->status);
            return true;
        },
        -10);

    // Registered SECOND, but runs FIRST (priority 100)
    eventus::subscribe<ModifiableEvent>(
        &b,
        [](ModifiableEvent* e) {
            e->value *= 2;
            e->status = "High Priority Doubled";
            std::println("  [Priority 100] High: value={}, status='{}'", e->value, e->status);
            return true;
        },
        100);

    // Registered THIRD, but runs FIFTH (priority -10, same as first subscriber)
    // This demonstrates that equal priorities maintain registration order
    eventus::subscribe<ModifiableEvent>(
        &b,
        [](ModifiableEvent* e) {
            e->value -= 3;
            e->status += " -> Another Low Subtracted 3";
            std::println("  [Priority -10] Low (2nd): value={}, status='{}'", e->value, e->status);
            return true;
        },
        -10);

    // Registered FOURTH, but runs SECOND (priority 50)
    eventus::subscribe<ModifiableEvent>(
        &b,
        [](ModifiableEvent* e) {
            e->value += 10;
            e->status += " -> Medium-High Added 10";
            std::println("  [Priority 50] Medium-High: value={}, status='{}'", e->value, e->status);
            return true;
        },
        50);

    // Registered FIFTH, but runs THIRD (priority 0)
    // This one conditionally stops propagation
    eventus::subscribe<ModifiableEvent>(&b, [](ModifiableEvent* e) {
        e->status += " -> Medium Checked";
        std::println("  [Priority 0] Medium: value={}, status='{}'", e->value, e->status);

        if (e->value > 25) {
            std::println(
                "  [Priority 0] Medium: STOPPING PROPAGATION (value {} exceeds threshold 25)",
                e->value);
            return false;  // Stop propagation
        }

        std::println("  [Priority 0] Medium: Continuing propagation");
        return true;
    });

    // Registered SIXTH, but runs LAST if it executes (priority -50)
    eventus::subscribe<ModifiableEvent>(
        &b,
        [](ModifiableEvent* e) {
            e->value += 100;
            e->status += " -> Lowest Added 100";
            std::println("  [Priority -50] Lowest: value={}, status='{}'", e->value, e->status);
            return true;
        },
        -50);

    std::println("=== Scenario 1: Full Propagation (starting value=5) ===");
    std::println("Expected: All handlers execute in priority order\n");
    eventus::publish(&b, ModifiableEvent{5, "Start"});

    std::println("\n=== Scenario 2: Stopped Propagation (starting value=10) ===");
    std::println("Expected: Medium handler stops propagation, low priority handlers don't run\n");
    eventus::publish(&b, ModifiableEvent{10, "Start"});

    std::println("\n=== Scenario 3: Partial Propagation (starting value=8) ===");
    std::println("Expected: Reaches some low priority handlers before stopping\n");
    eventus::publish(&b, ModifiableEvent{8, "Start"});

    std::println("\n=== Summary ===");
    std::println("Priority determines execution order: Higher numbers execute first");
    std::println(
        "Registration order: Low(-10) -> High(100) -> Low(-10) -> Med-High(50) -> Med(0) -> "
        "Lowest(-50)");
    std::println(
        "Execution order: High(100) -> Med-High(50) -> Med(0) -> Low(-10) -> Low(-10) -> "
        "Lowest(-50)");
    std::println("Any handler returning false stops propagation to lower priorities");

    return 0;
}