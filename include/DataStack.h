#ifndef DATASTACK_H
#define DATASTACK_H

#include "Singleton.h"
#include <stack>

// Purpose:    Implements low-level FORTH data stack operations
//             (push, pop, swap, depth, etc.)
//
// STACK USAGE PROTOCOL:
// ------------------------------------------------------------------------------------
// - This FORTH implementation uses a **full descending stack** for data.
// - **R15** is the **data stack pointer** (DSP).
// - **TOS (Top of Stack) is cached in R13**, reducing memory access.
// - **TOS-1 (Second from Top) is cached in R12**.
// - **TOS-2 and below** reside in **[R15]** and grow downward in memory.
//
// STACK LAYOUT:
// -------------------------------------------------------------------------------------
//     +-----------------+  <-- stackbase (highest address)
//     |   Unused Space  |
//     |      ...        |
//     |   Stack Data    |  (grows downward)
//     |      ...        |
//     |   TOS-2        |  [R15]  <-- Stack Pointer (R15)
//     |   TOS-1        |  (cached in R12)
//     |   TOS          |  (cached in R13)
//     +-----------------+  <-- stack grows downward (decrement R15)
//
// STACK OPERATIONS:
// --------------------------------------------------------------------------------------
// - **PUSH x**  → `sub r15, 8// mov [r15], r13// mov r13, x`
// - **POP x**   → `mov r13, [r15]// add r15, 8`
// - **DUP**     → ( x → x x )
// - **DROP**    → ( x → )
// - **SWAP**    → ( x1 x2 → x2 x1 )
// - **ROT**     → ( x1 x2 x3 → x2 x3 x1 )
// - **-ROT**    → ( x1 x2 x3 → x3 x1 x2 )
// - **OVER**    → ( x1 x2 → x1 x2 x1 )
// - **NIP**     → ( x1 x2 → x2 )
// - **TUCK**    → ( x1 x2 → x2 x1 x2 )
// - **2DUP**    → ( x1 x2 → x1 x2 x1 x2 )
// - **2DROP**   → ( x1 x2 → )
// - **DEPTH**   → ( -- n )  (Pushes stack depth onto the stack)
// - **PICK n**  → ( xn ... x1 n -- xn ... x1 xn )  (Copies nth element)
// - **ROLL n**  → ( xn ... x1 n -- x(n-1) ... x1 xn )  (Moves nth element to TOS)
//
// MEMORY ALIGNMENT REQUIREMENTS:
// ---------------------------------------------------------------------------------------
// - MacOS requires **16-byte stack alignment** when calling system functions.
// - R15 must always be **8-byte aligned** (for 64-bit values).
// - Stack allocation (`sub r15, 8`) and deallocation (`add r15, 8`) maintain alignment.
// - `depth` uses **RIP-relative addressing** (`lea rax, [rel stackbase]`) to comply
//   with x86-64 MacOS memory addressing restrictions.



class DataStack : public Singleton<DataStack> {
    friend class Singleton<DataStack>;

public:
    void push(int value);
    int pop();
    int top() const;

private:
    DataStack() = default;
    std::stack<int> stack;
};

#endif // DATASTACK_H
