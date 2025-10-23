# State Machine Example

Demonstrates LopCore type-safe hierarchical state machine with internal and external transitions.

## What This Shows

-   Type-safe enum-based state machine
-   State handlers with entry/exit/update lifecycle hooks
-   Internal transitions: States trigger their own transitions
-   External transitions: Application controls state changes
-   Transition validation rules
-   State change observers
-   State history tracking

## State Definitions

```cpp
enum class AppState {
    INIT,      // Initialization - self-transitions when complete
    RUNNING,   // Normal operation - externally controlled
    PAUSED,    // Suspended - externally controlled
    ERROR,     // Error recovery - self-transitions after recovery
    SHUTDOWN   // Final state
};
```

## Internal vs External Transitions

**Internal:** States know when they are done and transition themselves.

-   InitState: 3 steps then auto-transitions to RUNNING
-   ErrorState: 3 recovery attempts then auto-transitions to RUNNING

**External:** Main loop controls when to change states.

-   RUNNING to PAUSED: App decides to pause
-   PAUSED to RUNNING: App resumes
-   RUNNING to ERROR: App detects error
-   RUNNING to SHUTDOWN: App decides to shutdown

## Execution Flow

```
INIT --(internal)--> RUNNING --(external)--> PAUSED
PAUSED --(external)--> RUNNING --(external)--> ERROR
ERROR --(internal)--> RUNNING --(external)--> SHUTDOWN
```

## Building

```bash
cd examples/03_state_machine
idf.py build flash monitor
```

## Key Takeaways

-   Type-safe: Compile-time validation using enums
-   Flexible: Mix internal and external transition control
-   Encapsulated: Each state is a self-contained class
-   Observable: Track all state changes
-   Validated: Only allowed transitions succeed
-   Testable: Mock states for unit testing
