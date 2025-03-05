# ADR-004: Compatibility with Standards

## Context
To ensure our Forth implementation is portable, maintainable, and aligned with established best practices, we must adhere to an appropriate standard. The **Forth 2012 Standard** is the most recent and widely accepted specification, providing a well-defined foundation for Forth implementations while incorporating modern improvements.

Key aspects of the **Forth 2012 Standard** include:
- Enhanced **portability** across different systems.
- A well-defined **core word set** for consistency.
- Improved **extensibility** while maintaining backward compatibility with ANS Forth (1994).
- A clearly defined **separation of code and data** to improve execution efficiency and security.

## Decision
We have decided to align our Forth implementation with the **Forth 2012 Standard** while incorporating **modern JIT compilation** using **AsmJit**. Our approach includes:

1. **Code and Data Separation**
   - **Executable machine code** will be generated using **AsmJit**.
   - **Dictionary space** will contain **only data** (headers, names, variables, and pointers to structured heap data).
   - This ensures dictionary modifications do not interfere with generated JIT-compiled functions.

2. **JIT-Compiled Execution**
   - Every executable **function will be stored outside the dictionary**, in dynamically allocated memory.
   - JIT-generated functions will be called directly from their allocated locations, improving performance.
   - The dictionary will only store references to JIT-generated code, ensuring **safe and efficient execution**.

3. **Compatibility Considerations**
   - Standard **Forth words will be implemented in alignment with Forth 2012**.
   - **Control flow words (`IF`, `CASE`, `DO` loops)** will be adapted to JIT execution while maintaining standard behavior.
   - **Tail-call optimizations and register-based execution** will be leveraged where possible.

4. **Core Words to be Implemented**
   - **Stack Operations**: `DUP`, `DROP`, `SWAP`, `OVER`, `ROT`, `PICK`.
   - **Arithmetic and Logical Operations**: `+`, `-`, `*`, `/`, `MOD`, `AND`, `OR`, `XOR`, `INVERT`.
   - **Control Flow**: `IF...ELSE...THEN`, `BEGIN...UNTIL`, `DO...LOOP`, `CASE...OF...ENDOF...ENDCASE`.
   - **Memory and Variables**: `VARIABLE`, `CONSTANT`, `CREATE...DOES>`.
   - **I/O Operations**: `.`, `EMIT`, `KEY`, `ACCEPT`, `TYPE`.
   - **Dictionary Management**: `WORDS`, `FIND`, `EXECUTE`, `FORGET`, `IMMEDIATE`.
   - **System and Compilation Words**: `:` (colon definitions), `;`, `COMPILE,`, `[']`, `[COMPILE]`.
   - **String/Text Processing Words**: `COUNT`, `S>NUMBER?`, `CMOVE`, `CMOVE>`, `COMPARE`, `SEARCH`, `-TRAILING`.

### **5. Per-Word Data Space Management**
To ensure memory isolation and avoid global data space conflicts, we will implement:
- A **per-word heap** where each Forth word receives its **own dedicated memory allocation**.
- A **metadata system** that tracks **which word owns which allocation**.
- An efficient **lookup mechanism** to retrieve a word’s associated data block.
- **Automated cleanup** when a word is redefined or forgotten.

This approach:
- **Ensures encapsulation**: Each word has independent memory.
- **Improves safety**: Prevents accidental overwrites in shared space.
- **Supports garbage collection**: Enables precise deallocation.

Heap memory will be **separate from the dictionary space**, ensuring **JIT-generated code and word data remain independent**.



## Consequences
### ✅ **Pros**:
- **Improved Performance** → JIT-compiled functions run natively as machine code.
- **Better Memory Safety** → Code is executed separately from dictionary data.
- **Standard Compatibility** → Maintains some alignment with Forth 2012.
- **Portability** → Uses modern Forth conventions while leveraging AsmJit’s multi-platform capabilities.

### ❌ **Cons**:
- **Increased Complexity** → Requires careful management of JIT-compiled memory.
- **Debugger Complexity** → Debugging JIT-generated functions requires specialized tooling.

## Implementation Plan
1. Implement **JIT function allocation** separately from dictionary space.
2. Ensure **generated machine code aligns with Forth 2012 execution model**.
3. Adapt **CASE statements, loops, and branching logic** for JIT execution.
4. Validate compatibility using **Forth test suites for standard compliance**.
5. Implement and test core words listed above to ensure full compliance.

## Status
✅ **Accepted**

## References
- [Forth 2012 Standard](https://forth-standard.org/)
- [AsmJit Documentation](https://github.com/asmjit/asmjit)
