# ADR-001: Use Singleton Pattern for Core Components

## Context
In our FORTH JIT system, multiple core components (Interpreter, Compiler, DataStack, etc.)
need global access while ensuring only a single instance exists.

## Decision
We will implement the Singleton pattern for these components to:
- Ensure a single shared instance.
- Avoid excessive dependencies and manual object management.
- Provide a structured way to manage shared state.

## Consequences
### ✅ Pros:
- Simplifies component access.
- Ensures consistency in shared data.
- Reduces memory overhead by avoiding duplicate instances.

### ❌ Cons:
- Introduces global state.
- Can complicate unit testing (requires mocking).

## Status
Accepted

## References
- [Singleton Design Pattern](https://en.wikipedia.org/wiki/Singleton_pattern)
