# ADR-005: Code Generation Approach

## Context
To ensure efficient execution of our Forth implementation, we need a structured approach to code generation. The chosen approach must be:
- **Fast**: Generating optimized machine code for execution.
- **Portable**: Supporting different architectures where possible.
- **Maintainable**: Easy to modify and extend.
- **Compliant**: Aligning with the Forth 2012 standard.

## Decision
We have decided to use **AsmJit** for **JIT (Just-In-Time) compilation** of Forth words into executable machine code. This approach allows:
- **Efficient execution of compiled Forth words**.
- **On-the-fly generation of machine code**.
- **Direct function pointers for execution**.
- **Dynamic runtime code modification**.

 