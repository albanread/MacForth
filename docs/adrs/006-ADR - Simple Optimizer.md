# ADR-006: Optimizer

## Context
To improve the performance of our **JIT-compiled Forth implementation**, 
we will introduce **optimizations** at the **token stream level** before code generation. 
The goal is to:
- **Eliminate unnecessary stack operations**.
- **Reduce instruction count** by optimizing common sequences.
- **Utilize our TOS (Top of Stack) cache in R13** for efficiency.
- **Enable constant folding** by detecting **`NUMBER OPERATOR`** patterns.
- **Apply strength reduction** for **multiplications and divisions** by powers of 2.

## Decision
We will implement an **optimizer** that processes the stream of **tokens** from the **tokenizer** before JIT compilation. 
This optimizer:
1. **Recognizes `NUMBER OPERATOR` patterns** and replaces them with **immediate instructions**.
2. **Performs peephole optimizations** to simplify **stack operations**.
3. **Eliminates redundant instructions** such as unnecessary **`SWAP DROP`** sequences.
4. **Implements Tail Call Optimization (TCO)** to avoid function call overhead when possible.

## **Optimizations Implemented**

### **1️⃣ Constant Folding (Arithmetic Simplification)**
Instead of:
```assembly
mov r13, 10
push r13
add r13, 2
```
We will directly **apply the operation to R13**:
```assembly
add r13, 2
```
#### **Optimized Patterns**
| **Token Pair** | **Optimized Code** |
|---------------|-------------------|
| `10 +`       | `add r13, 10` |
| `4 *`        | `shl r13, 2` (since 4 = 2^2) |
| `2 /`        | `shr r13, 1` (since 2 = 2^1) |
| `8 /`        | `shr r13, 3` (since 8 = 2^3) |

---

### **2️⃣ Operator Fusion (Strength Reduction)**
Detect sequences that can be replaced with **faster operations**.

#### **Example: `DUP +`**
Instead of:
```assembly
sub r15, 8
mov [r15], r13
mov r12, r13
add r13, r12
```
We will **optimize**:
```assembly
lea r13, [r13 + r13]  ; Optimized DUP + (TOS = 2 * TOS)
```

#### **Example: `DUP *`**
Instead of:
```assembly
mov r12, r13
mul r13, r12
```
We will **use bitwise shifts**:
```assembly
shl r13, 1 ; Equivalent to multiplying by 2
```

---

### **3️⃣ Peephole Optimization (Stack Manipulation)**
Detect and **simplify redundant stack operations**.

#### **Example: `SWAP DROP`**
- `SWAP`:
```assembly
mov rax, r12
mov r12, r13
mov r13, rax
```
- `DROP`:
```assembly
mov r13, r12
```
- **Optimized**:
```assembly
mov r13, r12  ; Equivalent to DROP (ignoring SWAP)
```

#### **Example: `OVER OVER`**
Instead of:
```assembly
mov rax, r12
mov r12, r13
mov r13, rax
mov [r15-8], r13
```
We will **directly optimize**:
```assembly
mov rax, r13
mov r12, r13
```

---

### **4️⃣ Tail Call Optimization (TCO)**
Avoid unnecessary **return and call overhead** when the last operation in a function is a **call to another word**.

#### **Example:**
Instead of:
```assembly
call someFunction
ret
```
We will **use jump**:
```assembly
jmp someFunction
```
This eliminates **stack overhead** and improves execution speed.

---

### **5️⃣ Optimization Rules for Token Stream Processing**
These rules will be applied in `optimize_constant_operations()` before JIT compilation:

#### **Rule 1: Constant Folding**
If a **number** is immediately followed by an **operator**, replace it with an **immediate instruction**.
```cpp
if (tokens[i].type == TOKEN_NUMBER && (tokens[i+1].value == "+" || tokens[i+1].value == "-" || tokens[i+1].value == "*" || tokens[i+1].value == "/")) {
    int64_t constant = tokens[i].int_value;
    std::string op = tokens[i+1].value;
    
    if (op == "+") {
        optimized_tokens.emplace_back(TOKEN_OPTIMIZED, "ADD_IMM", constant);
    } else if (op == "-") {
        optimized_tokens.emplace_back(TOKEN_OPTIMIZED, "SUB_IMM", constant);
    } else if (op == "*") {
        if ((constant & (constant - 1)) == 0) {
            optimized_tokens.emplace_back(TOKEN_OPTIMIZED, "SHL_IMM", __builtin_ctz(constant));
        } else {
            optimized_tokens.emplace_back(TOKEN_OPTIMIZED, "MUL_IMM", constant);
        }
    } else if (op == "/") {
        if ((constant & (constant - 1)) == 0) {
            optimized_tokens.emplace_back(TOKEN_OPTIMIZED, "SHR_IMM", __builtin_ctz(constant));
        } else {
            optimized_tokens.emplace_back(TOKEN_OPTIMIZED, "DIV_IMM", constant);
        }
    }
    i++; // Skip operator token
    continue;
}
```

---

## **Status**
✅ **Accepted**

## Implementation Notes

The FORTH compiler works, by running code generation words, these words are all in the dictionary.

The definitions of these words are either written in C or assembly language and they typically generate code.

The OPTIMIZER identifies opportunities for substituting sequences of standard words and numbers in the input stream.

Optimizer reviews the tokenized code and updates the tokens with new words or word sequences that generate more efficient code under specific 
circumstances.

These optimizing words are also words in the dictionary stored in the FRAGMENTS vocabulary, named NNN_IMM.


## **References**
- [Forth 2012 Standard](https://forth-standard.org/)
- [AsmJit Documentation](https://github.com/asmjit/asmjit)
