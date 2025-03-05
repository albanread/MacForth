# ADR-009: Infix Evaluator with LET for Floating Point Expressions

## Context
Modern Intel Xeon hardware includes numerous XMM floating-point registers. 
Traditional stack-based FORTH excels at simple integer arithmetic and parameter passing but struggles with efficiently evaluating floating-point expressions. To improve performance, we propose introducing an infix evaluator word, `LET`, that computes expressions using floating-point arithmetic while integrating seamlessly with FORTH's stack-based nature.

## Proposal
The `LET` word introduces a new syntax for evaluating floating-point expressions using infix notation. It allows variable assignments via `WHERE` clauses, ensuring that dependent computations are resolved before the final computation. Additionally, `LET` supports parameter passing via the data stack and stores computed results back onto the stack.

The `LET` statement allows lightweight, efficient evaluation of floating-point expressions with local variable definitions. These variables are:
- **Transient**: Variables used in the `LET` statement are local to the `LET` block and exist only for the duration of its execution. These variables are unrelated to traditional FORTH variables and cannot be accessed outside of the `LET` scope.
- **Forth Stack Integration**: All results from the `LET` statement are pushed back onto the FORTH stack after computation, making them seamlessly usable in subsequent FORTH operations.
- **Local Definitions**: Variables may be initialized either from values on the FORTH data stack or using `WHERE` clauses within the `LET` statement.

Additionally, the compiled code for a `LET` statement will form part of a compiled FORTH word. The `LET` statement **cannot** be executed interactively or in immediate ("interpreted") mode—it is restricted to use inside compiled words to ensure efficient reuse and execution.

### Syntax
The proposed syntax for `LET` follows:

```forth
LET (x, y, z) = FN(a, b, c) = result_expr WHERE y = expr1 WHERE v = expr2 WHERE k = expr3;
```

- `LET (...)` defines output variables.
- `FN(...)` defines input parameters sourced from the data stack.
- The final expression computes a result using floating-point operations.
- `WHERE` clauses are evaluated sequentially to define intermediate variables.
- The expressions utilize up to 26 floating-point variables (`a` through `z`), maximizing register usage.

## Compilation Strategy
To achieve efficient execution, `LET` expressions should be compiled to native machine code using **ASMJIT**. The compilation process involves:

1. **Tokenizing** the `LET` statement into identifiable components:
    - Output variables
    - Function parameters
    - Infix expressions
    - `WHERE` clauses

2. **Register Allocation:**
    - Utilize available XMM registers to store variables.
    - Prioritize keeping frequently used values in registers.
    - Spill values to memory only when necessary.

3. **Generating Assembly with ASMJIT:**
    - Convert parsed expressions into an Abstract Syntax Tree (AST).
    - Optimize instruction sequences for efficient execution.
    - Generate machine code dynamically.

4. **FORTH Integration:**
    - Ensure parameter passing aligns with FORTH's stack conventions.
    - Push final computed values back onto the stack.
    - The `LET` word operates as part of a compiled FORTH word. Once defined, the JIT-compiled code for a `LET` statement becomes a permanent part of the compiled word, ensuring that it is only JIT-compiled once.
    - As `LET` is restricted to compiled words, this avoids the performance overhead of re-evaluating the JIT compilation step for repeat executions.
    - Results from the `LET` block are returned to the FORTH data stack after execution, ensuring compatibility with subsequent stack-based operations.

## Considerations

### Performance
- Maximizing register usage is critical for speed.
- ASMJIT provides efficient runtime compilation.

### Syntax Simplicity
- While infix notation is a departure from FORTH’s postfix tradition, it offers significant readability and usability advantages.
- The use of `WHERE` maintains clarity and structure.

### Stack Compatibility
- Ensure seamless interaction between `LET`-compiled code and traditional FORTH words.
- Allow computed results to be accessed in FORTH expressions without friction.

### Scoping & Execution Rules
- Variables defined in a `LET` statement (`a` through `z`) are **local and transient**. They exist solely within the scope of the `LET` block and are discarded once the block completes execution.
- These variables do not interfere with or persist as FORTH variables elsewhere in the system, ensuring clean isolation of the `LET` environment.
- Input variables are initialized either from values on the FORTH stack (using the `FN(...)` input definition) or using a `WHERE` clause within the `LET` syntax.
- As `LET` results are pushed back onto the FORTH stack after execution, they can be used seamlessly by other FORTH words.

Additionally:
- Any `LET` syntax must exist within a **compiled FORTH word**. Interactive or interpreted execution of `LET` is not allowed. This restriction ensures that the code can benefit fully from **one-time JIT compilation** and avoids runtime overhead.

### Error Handling
- Variables initialized via the FORTH stack or defined using `WHERE` clauses must handle errors gracefully.
- Errors such as using undefined variables or attempting to execute `LET` in interpreted mode will result in clear runtime or compilation-time errors.

### JIT Storage
- The JIT compiler will integrate `LET` functionality directly into the compiled FORTH word, ensuring efficient execution for repeated invocations. Addressing how multi-word access or compilation errors might behave remains an important part of the prototype.

## Example Applications and Tests
To ensure correctness and validate performance, we define a set of test cases:

### Example 1: Basic Arithmetic
```forth
LET (x) = FN(a, b) = a + b WHERE a = 5.0 WHERE b = 3.0; 
```
**Expected Result:** `x = 8.0`

### Example 2: Quadratic Formula
```forth
LET (x1, x2) = FN(a, b, c) = (-b + sqrt(bb - 4ac)) / (2a), (-b - sqrt(bb - 4ac)) / (2a) 
WHERE a = 1.0 WHERE b = -3.0 WHERE c = 2.0; 
```
**Expected Result:** `x1 = 2.0, x2 = 1.0`

### Example 3: Trigonometric Functions
```forth
LET (y) = FN(x) = sin(x) + cos(x) WHERE x = 0.5; 
```
**Expected Result:** `y ≈ 1.357`

### Example 4: Exponential and Logarithm
```forth
LET (z) = FN(x) = exp(x) - ln(x) WHERE x = 2.0; 
```
**Expected Result:** `z ≈ 6.6106`

### Example 5: Complex Expression
```forth
LET (r) = FN(x, y) = (x^2 + y^2)^0.5 WHERE x = 3.0 WHERE y = 4.0; 
```
**Expected Result:** `r = 5.0`

**Important Note:**  
Variables like `a`, `b`, and `c` used in `LET` examples are not persistent FORTH variables. They exist only during `LET` execution and are discarded afterward. The computed results are returned as normal FORTH stack values.  
Additionally, the examples assume execution within a **compiled word** environment, as `LET` cannot be executed interactively.

### Example in Practice
```forth
: compute-hypotenuse
    LET (r) = FN(x, y) = (x^2 + y^2)^0.5 
        WHERE x = 3.0 
        WHERE y = 4.0 ; 
    r . ; Print the result (should print 5.0)
;
```

---


## Tokenization: LetLex
To efficiently parse `LET` statements and generate an Abstract Syntax Tree (AST), we introduce `LetLex`, a tokenizer that converts statements into token lists.

### Token Recognition
`LetLex` identifies the following tokens:
- Keywords: `LET`, `FN`, `WHERE`.
- Parentheses: `(`, `)`.
- Operators: `=`, `+`, `-`, `*`, `/`, `^`, `sqrt`, `sin`, `cos`, `exp`, `ln`.
- Variables: `a-z`.
- Constants: Numeric literals (e.g., `3.14`, `2.0`).
- Delimiters: `,`, `;`.

### Example Tokenization
#### Input
```
LET (x, y) = FN(a, b) = a + b * sqrt(a) WHERE b = 2.0;
```
#### Tokenized Output
```
[
  ('LET', 'KEYWORD'), ('(', 'PAREN'), ('x', 'VAR'), (',', 'DELIM'), ('y', 'VAR'), (')', 'PAREN'),
  ('=', 'OP'), ('FN', 'KEYWORD'), ('(', 'PAREN'), ('a', 'VAR'), (',', 'DELIM'), ('b', 'VAR'), (')', 'PAREN'),
  ('=', 'OP'), ('a', 'VAR'), ('+', 'OP'), ('b', 'VAR'), ('*', 'OP'), ('sqrt', 'FUNC'), ('(', 'PAREN'), ('a', 'VAR'), (')', 'PAREN'),
  ('WHERE', 'KEYWORD'), ('b', 'VAR'), ('=', 'OP'), ('2.0', 'NUM'), (';', 'DELIM')
]
```




## Open Questions
- Should `LET` support inline definitions (`LET x = y + z;`) without function notation? *NO*
- How should error handling be managed (e.g., division by zero, undefined variables)? *USING exceptins*
- Should `LET` support vectorized operations utilizing AVX instructions? *yes*

---

## Recommendations
1. **Prototype Implementation:** Develop a minimal `LET` compiler using ASMJIT to evaluate feasibility.
2. **Benchmark Performance:** Compare floating-point expression evaluation against traditional FORTH methods.
3. **Iterate on Syntax:** Experiment with variations in syntax to ensure usability.

---

## Decision
Pending review of prototype results.