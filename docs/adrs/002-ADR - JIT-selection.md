# ADR-002: Selection of JIT System

## Change in JIT Strategy

### **Reasons for Switching to AsmJit**:
- **Prebuilt x86-64 Instruction Set**: No need to manually define instructions.
- **Easier API**: Provides a high-level C++ interface for JIT compilation.
- **Cross-Platform Support**: Works on macOS, Linux, and Windows.
- **Better Maintainability**: Reduces complexity compared to DynASM.
- **Robust Memory Management**: Uses built-in JIT runtime allocation.
- **Logging & Debugging**: Supports **AsmJit::FileLogger** for tracking generated code.
