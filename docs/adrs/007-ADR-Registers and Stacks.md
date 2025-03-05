# **ADR 007: Register and Stack Usage for Intel macOS**

---

### **Stack Usage Protocol**

```c++
// STACK USAGE PROTOCOL:
// ------------------------------------------------------------------------------------
// - This FORTH implementation uses a **full descending stack** for data.
// - **R15** is the **data stack pointer** (DSP).
// - **TOS (Top of Stack) is cached in R13**, reducing memory access.
// - **TOS-1 (Second from Top) is cached in R12**.
// - **TOS-2 and below** reside in **[R15]** and grow downward in memory.

//

```
// - All stacks use memory allocated via `malloc`, so their addresses are determined dynamically.
---

# **1. Context**
This document establishes the **register and stack usage conventions** for the JIT-compiled Forth implementation on Intel-based macOS systems. Given the limited availability of registers and the necessity for multiple stacks—including the **data stack**, **return stack**, **floating-point stack**, and **locals management**—careful allocation is required for efficient execution and adherence to the Forth execution model.

The **Stack Usage Protocol** ensures efficient memory usage and minimizes operations involving main memory by caching the **Top of Stack (TOS)** and **Second from Top** in registers.

---

### **2. Register Allocation**

The following registers are allocated for specific purposes:

| **Register** | **Usage**                                      |
|--------------|------------------------------------------------|
| `R15`        | **Data Stack Pointer (DSP)**                  |
| `R13`        | **Top of Stack (TOS)**                        |
| `R12`        | **Second from Top (TOS-1)**                   |
| `R14`        | **Return Stack Pointer (RSP)**—Utilized for DO..LOOP counters and subroutine returns |
| `XMM0-XMM3`  | **Floating-point Top-of-Stack Registers (FPS)**—Used for caching floating-point values |

Each register serves a specific role in minimizing memory access and ensuring efficient operations across stacks. The **data stack usage protocol** ensures optimized register usage by caching `TOS` and `TOS-1` in `R13` and `R12` accordingly.

---

### **3. Data Stack (`R15`-based)**

- **Growth Direction:** The **data stack** grows **downward** in memory.
- **TOS Cache Design:**
    - **`R13`:** Caches **Top of Stack (TOS)** to reduce memory accesses.
    - **`R12`:** Caches **Second from Top (TOS-1)**.
- **Memory Operations:**
    - **Push:** Values are stored to `[R15]`, and `R15` is decremented.
    - **Pop:** Values are read from `[R15]`, and `R15` is incremented.

#### **Revised Example Usage (Protocol-Compliant):**
```asm
// Push an item (value in RAX) to the data stack
mov R12, R13         // TOS -> TOS-1 (R12 = R13)
mov R13, RAX         // New TOS (RAX -> R13)
sub $8, R15          // Decrement stack pointer
mov (R15), R12       // Store old TOS-1 in [R15]

// Pop an item into RBX (pop into a general register)
mov R12, (R15)       // Load old TOS-1 from memory ([R15] -> R12)
add $8, R15          // Increment stack pointer
mov RBX, R13         // Copy TOS to RBX
mov R13, R12         // TOS = TOS-1
```

**Key Recommendation:** Avoid direct memory access whenever possible by maintaining `TOS` (`R13`) and `TOS-1` (`R12`) in registers during operations.

---

### **4. Return Stack (`R14`-based)**

The **return stack** is used for:
1. **Control Structures (DO..LOOP):** Index and limit values.
2. **Program Subroutine Returns:** Storing return addresses for `CALL`/`RET` operations.

#### **Protocol Compliance for Pushing and Popping:**
```asm
// Push index (RCX) and limit (RDX) onto return stack
sub $16, R14         // Reserve space on return stack
mov RDX, (R14)       // Store limit at TOS
mov RCX, 8(R14)      // Store index (TOS-1)

// Pop index and limit from return stack
mov RDX, (R14)       // Retrieve limit
mov RCX, 8(R14)      // Retrieve index
add $16, R14         // Restore stack pointer
```

**Critical:** The **data stack (`R15`)** and the **return stack (`R14`)** must be kept separate to prevent collisions. Implement bounds-checking logic or allocate dedicated stack memory regions as required.

---

### **5. Floating-Point Support**

#### **5.1. XMM Register Usage**
- **XMM0-XMM3:** Reserved as a **floating-point stack**, minimizing spills to memory.
- Additional floating-point values beyond XMM0-XMM3 are spilled to the **data stack (`[R15]`)**.
- **SSE Instructions** (e.g., `ADDSD`, `MULSD`) are used for floating-point arithmetic.

#### **Revised Example (Floating-Point Addition):**
```asm
// TOS += TOS-1 using floating-point registers
movsd (R15), XMM1    // Load TOS-1 from stack into XMM1
add $8, R15          // Pop TOS-1
addsd XMM1, XMM0     // Add XMM0 (TOS) += XMM1 (TOS-1)
movsd XMM0, R13      // Update TOS to XMM0
```

#### **Key Recommendations:**
1. Use **`XMM0-XMM3`** for floating-point stack cache.
2. Spill excess values to `[R15]` only when more than four floating-point values are being manipulated.

---

### **6. Local Variable Management**

#### **6.1 Small Locals Using Return Stack (`R14`)**
- **Small or temporary local variables** can be efficiently managed using the return stack.

##### **Protocol-Compliant Example:**
```asm
sub $8, R14          // Reserve space for local variable
mov RAX, (R14)       // Store local variable

// Retrieve local variable
mov (R14), RAX       // Load local variable into RAX
add $8, R14          // Release allocated space
```

#### **6.2 Complex Locals Using Stack Frames (`RBP`)**
- For **large or complex locals**, use a dedicated stack frame based on `RBP`.

##### **Example with Stack Frame:**
```asm
push RBP             // Save old base pointer
mov RSP, RBP         // Establish new frame
sub $32, RSP         // Allocate 32 bytes for locals

// Access local variable
mov RAX, -8(RBP)     // Store value in local space
```

---

### **7. Subroutine Calls and Return**

- Subroutine calls use the **native `CALL` and `RET` instructions** that leverage the conventional `RSP` stack.
- Ensure that `R14` (return stack) is completely independent from the subroutine call stack.

---

### **8. Memory Layout Diagram**
// - All stacks use memory allocated via `malloc`, so their addresses are determined dynamically.

---

### **9. Summary and Recommendations**

| **Feature**               | **Recommendation**                                                         |
|----------------------------|---------------------------------------------------------------------------|
| **DO..LOOP**               | Use `R14` as **return stack pointer** to store loop indices and limits.    |
| **Data Stack Management**  | Adhere to the **full descending stack** with `TOS` in `R13` and `TOS-1` in `R12`. |
| **Floating-Point Support** | Use `XMM0-XMM3` for floating-point caching; spill to `[R15]` only when necessary. |
| **Local Variables**        | Use `R14` for small locals; use stack frames for complex locals.            |
| **Stack Integrity**        | Prevent collisions between **data stack (`R15`)**, **return stack (`R14`)**, and **subroutine stack (`RSP`)**. |
