# ADR-002: Selection of JIT System

## Change in JIT Strategy

### **Reasons for Switching to AsmJit**:
- **Prebuilt x86-64 Instruction Set**: No need to manually define instructions.
- **Easier API**: Provides a high-level C++ interface for JIT compilation.
- **Cross-Platform Support**: Works on macOS, Linux, and Windows.
- **Better Maintainability**: Reduces complexity compared to DynASM.
- **Robust Memory Management**: Uses built-in JIT runtime allocation.
- **Logging & Debugging**: Supports **AsmJit::FileLogger** for tracking generated code.

## Testing Framework Selection
Several C++ testing frameworks were evaluated:

| **Testing Framework** | **Unit Tests** | **Integration Tests** | **Ease of Use** | **Platform Support** | **Best Use Case** |
|----------------------|---------------|----------------------|----------------|----------------|----------------|
| **Google Test (gtest)** | ✅ Yes | ✅ Yes | ✅ Easy | ✅ macOS/Linux | General-purpose testing |
| **Catch2** | ✅ Yes | ✅ Yes | ✅ Very Easy | ✅ macOS/Linux | Header-only, simple |
| **Doctest** | ✅ Yes | ⚠️ Limited | ✅ Very Easy | ✅ macOS/Linux | Minimalist, for small projects |
| **Boost.Test** | ✅ Yes | ✅ Yes | ❌ Complex | ✅ macOS/Linux | Heavyweight, requires Boost |
