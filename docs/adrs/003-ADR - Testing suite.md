# ADR-002: Selection of Testing Framework for FORTH JIT System

## Context
To ensure correctness, stability, and maintainability of our FORTH JIT system, we need a robust testing framework that covers:

- **Unit Tests**: Verifying individual components such as `JitContext`, `CodeGenerator`, and `Interpreter`.
- **Integration Tests**: Ensuring multiple components work together (e.g., JIT compiling a FORTH word and executing it).
- **End-to-End Tests**: Simulating user input in the REPL and validating results.
- **Performance Tests**: Measuring execution speed of JIT vs. interpreted execution.
- **Cross-Platform Support**: Ensuring tests run on both **x86-64 (Intel)** and **ARM64 (Apple Silicon)**.

Several C++ testing frameworks were evaluated:

| **Testing Framework** | **Unit Tests** | **Integration Tests** | **Ease of Use** | **Platform Support** | **Best Use Case** |
|----------------------|---------------|----------------------|----------------|----------------|----------------|
| **Google Test (gtest)** | ✅ Yes | ✅ Yes | ✅ Easy | ✅ macOS/Linux | General-purpose testing |
| **Catch2** | ✅ Yes | ✅ Yes | ✅ Very Easy | ✅ macOS/Linux | Header-only, simple |
| **Doctest** | ✅ Yes | ⚠️ Limited | ✅ Very Easy | ✅ macOS/Linux | Minimalist, for small projects |
| **Boost.Test** | ✅ Yes | ✅ Yes | ❌ Complex | ✅ macOS/Linux | Heavyweight, requires Boost |

## Decision
We have decided to use **Google Test (gtest)** for our testing framework because:

- **Comprehensive feature set**: Supports **unit, integration, and performance testing**.
- **Active development**: Maintained by Google, widely adopted.
- **Cross-platform compatibility**: Works on **macOS (Intel & ARM64)**.
- **CI/CD Ready**: Can be integrated into **GitHub Actions, Jenkins, or GitLab CI**.
- **Mature assertions API**: Provides clear, readable test output.

### **Alternatives Considered**
1. **Catch2**: Simple and header-only, but lacks some features like mocking.
2. **Doctest**: Minimalist, but lacks integration features.
3. **Boost.Test**: Powerful but **too heavy for our needs**.

## Consequences
### ✅ **Pros**:
- **Well-documented and actively maintained**.
- **Supports test fixtures and parameterized tests**.
- **Easy setup via CMake**.

### ❌ **Cons**:
- **Requires linking a separate library** (unlike header-only frameworks like Catch2).
- **Slightly more complex than minimalist alternatives**.

## Implementation Plan
1. **Integrate Google Test into CMake**.
2. **Write unit tests for core components** (`JitContext`, `CodeGenerator`).
3. **Create integration tests for JIT compilation and execution**.
4. **Automate tests in CI/CD pipelines**.

## Status
✅ **Accepted**

## References
- [Google Test Documentation](https://github.com/google/googletest)
- [Catch2 Documentation](https://github.com/catchorg/Catch2)
- [Boost.Test Documentation](https://www.boost.org/doc/libs/release/libs/test/)
