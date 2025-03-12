# Floating Point Expression Evaluator Documentation

## Overview

The floating-point expression evaluator is an extension feature of the `ForthJIT` project, 
responsible for compiling and executing floating point arithmetic expressions in a just-in-time (JIT) manner. 

The evaluator processes **input expressions**, resolves interdependent variables using `WHERE` clauses, allocates registers and memory dynamically,
and generates assembly-level code for execution.

This document provides an overview of the design, architectural components, key logic, and their interactions within the evaluator.

FORTH supports floating point operations, floating point values may be pushed to the stack, used by F+, F* etc and printed with F.

However there is a lot of stack overhead, and quite sparse computation taking place, it is also postfix FORTH notation.

The idea is to create denser code, than the normal forth floating point code, to make use of the machines abundant floating point registers, and to allow for more readable infix expressions.

The expression evaluator is less than perfect and fails to evaluate some expressions, in which case you will need to simplify them.

The complexity is around temporary expression register spillage, but if we cause too much of that we are not fast anyway.

The main issue is that the codegenerator is currently as dumb as a rock (or dumber tyh, with no optimization tree phase, so it tends to create too many
temporary expression registers.


You define a FORTH word to hold each LET expression.

``` 
: area
     LET (area) = FN(radius) = pi * pow( radius, 2 ) 
        WHERE pi = 3.141592653589793 ; 
        
1.0 area f. 

3.14159 
Ok 

: T7 let (x) = FN(n) = 2 * sqrt(n) ;


: T2      let (area) = fn(radius) = pi * pow(radius, 2) 
               where r_squared = pow(radius, 2)
               where pi = 3.141592653589793;

: T3 let (area) = fn(radius) = pi * pow(radius, 2)
   where gravity = 9.81
   where pi = 3.141592653589793;
   
: T4 let (area) = fn(radius) = pi * pow(radius, 2)
   where gravity = 9.81
   where pi = 3.141592653589793;
   
: T5 let (area) = fn(radius) = pi * pow(radius, 2)
   where pi = 3.14
   where pi = 3.141592653589793;
   
: T6
    let (area) = fn(radius) = pi * pow(radius, 2)
   where gravity = pow(3, 2)
   where pi = 3.141592653589793;
   
 : T7 let (area) = fn(radius) = gravity * pow(radius, 2)
   where gravity = 9.81
   where force = gravity * 2;  
   
: T9 let (force) = fn(radius) = gravity * radius
   where gravity = mass * 9.81
   where mass = 2;
   
 : T10 
 let (energy) = fn(radius) = gravity * velocity
   where gravity = mass * 9.81
   where velocity = radius * 2
   where mass = 5;  
   
 : T3
 let (kinetic_energy) = fn(velocity) = 0.5 * mass * pow(velocity, 2)
   where mass = weight / gravity
   where weight = 100
   where gravity = 9.81;  
   
   : ERR1 
   let (force) = fn(acceleration) = mass * acceleration
   where mass = gravity * factor
   where gravity = 9.81;
   
   : ERR2 
   let (energy) = fn(radius) = gravity * mass
    where gravity = 9.81;
   
   : ERR 4 
   let (area) = fn(radius) = pi * pow(radius, 2) 
        where pi = 3.141592653589793 
        where radius = 42;  ;
   
: building_height
     LET (height) = FN(shadowLength, angleDegrees) = shadowLength * tan(angleRadians)
      WHERE angleRadians = angleDegrees * pi / 180
      WHERE pi = 3.141592653589793
        ;
              
 10. 45. building_height f.
              
 \ sine wave position              
 : sine_wave_p 
     LET (position) = FN(amplitude, frequency, time) = amplitude * sin(2 * pi * frequency * time)
        WHERE pi = 3.141592653589793 ;
        
   1.0 1.0.0.0 sine_wave_p f. 0
   3.0 2.0 0.125 sine_wave_p  f. 3.0
        
 : point_distance
     LET (distance) = FN(x1, y1, x2, y2) = hypot(x2 - x1, y2 - y1) ;
     
 100. 100. 200. 200. point_distance f.
 141.421     
     
 : compound 
     LET (futureValue)
     = FN(principal, rate, years) = principal * pow(1 + rate, years) ;             
     
    100.0 0.0045 10.0 compound f.
    104.592 
    
 : deg
     LET (radians) = FN(degrees) = degrees * pi / 180
        WHERE pi = 3.141592653589793 ;
        
  : rad
     LET (degrees) = FN(radians) = radians * 180 / pi
        WHERE pi = 3.141592653589793
        
  : test 
     LET (x) = FN(a, b, c) = a + b + c ;     
    
   
 : T LET (a,b,c,d)=FN(a,b,c,d) = a * 2, b * 2, c * 2, d * 2 ;

```

test takes two floats from the FORTH stack, and returns one.

Multiple independent statements, seperated by commas, return multiple values, e.g. ```LET ( x, y z) = ``` so several calculations can be performed in one LET statement.

LET has its own lexer, parser, and code generator, and is considerably more complicated than the normal FORTH compiler, so here is the documentation.

An executing expression, can use values fed in from the FORTH stack, and also values calculated by WHERE statements.


## Features
- **Arithmetic Expression Evaluation**: Evaluate mathematical expressions, including multiple operands and nested operations, e.g., `a + b * c`.
- **Dynamic Register and Memory Allocation**: Efficiently allocate CPU registers and spill slots for temporary variables and intermediate computations.
- **Variable Resolution**: Support `WHERE` clauses for assigning variables and inline constants dynamically to expressions, e.g., `LET (x) = FN(a, b) = a + b WHERE a = 5.0 WHERE b = 3.0;`.
- **On-the-Fly Compilation**: Generates machine code optimized for floating-point operations using JIT.
- **Thread Safety**: Ensures isolation of memory and data for concurrent execution.

## Design
The evaluator is composed of several interconnected modules, each with a distinct role in parsing, compiling, and executing expressions. Below, the overall design and flow are outlined.
### 1. **Key Concepts**
- **LET Statement**: The primary construct to define expressions with variables and inline assignments. Example:
``` forth
  LET (x) = FN(a, b) = a + b WHERE a = 5.0 WHERE b = 3.0;
```
- `LET`: Introduces an equation or function.
- `FN`: Defines a function or expression to evaluate.
- `WHERE`: Declares variable definitions or inline values.

- **Memory Management**: Registers are allocated dynamically for variables and computational results. Spill slots in global memory are used when registers are insufficient.
- **JIT Execution**: The generated machine code is dynamically compiled, optimized, and executed to evaluate the expressions efficiently.

### 2. **System Components**
#### 2.1. **Parser**
The parser processes `LET` statements and builds an abstract syntax tree (AST) for the input expression.
- **Responsibilities**:
    - Parse expressions, variables, and `WHERE` clauses into a structured `LetStatement` object.
    - Validate expression format and detect syntax errors.

#### 2.2. **Expression Representation**
- Expressions such as `a + b` or `c * d` are represented as objects within the AST, enabling recursive evaluation and code generation.
- Example hierarchy in AST:
``` 
  Expression: a + b
      └── Operand: a
      └── Operand: b
```
#### 2.3. **Register Allocation**
Registers are assigned dynamically for storing variables and intermediate computation results.
- **Register Map**: A mapping between variables and available CPU registers. Example:
``` 
  registerMap = { "a": xmm0, "b": xmm1, "result": xmm2 }
```
- **Spill Slot Management**: When registers are full, spill slots in global memory are used for overflow handling.

#### 2.4. **Code Generator**
Generates assembly code for efficient floating-point operations using a JIT assembler framework (e.g., AsmJit).
- **Supports Arithmetic Operations**:
    - Addition: `+`
    - Subtraction: `-`
    - Multiplication: `*`
    - Division: `/`

- **Preallocation**: Variables are preallocated registers or memory before code emission.
- **Instruction Emission**: Assembly instructions for arithmetic operations:
``` 
  movaps xmm0, [a]     // Load a into xmm0
  addps  xmm0, [b]     // Add b to xmm0
  movaps [result], xmm0 // Store result
```
#### 2.5. **Memory Management**
- **Thread-Local Memory**: Global static memory pools are defined with `thread_local` to ensure thread safety during the evaluation.
``` 
  thread_local uint8_t gGlobalStaticMemory[GLOBAL_MEMORY_SIZE];
```
- **Variable Offsets**: Variables are assigned specific offsets in global/thread-local memory for storage and retrieval.

#### 2.6. **Execution Unit**
After JIT compilation, the generated function is executed to compute the floating-point result.
### 3. **Key Algorithms**
#### 3.1. **Variable Resolution with `WHERE` Clauses**
- `WHERE` clauses assign values or replace variables inline before evaluation, ensuring expressions like `a + b` can resolve `a` and `b` correctly.
- Algorithm:
    1. Parse all `WHERE` clauses, creating a mapping of variable values.
    2. Preallocate registers for `WHERE` variables (`a` → `xmm0`, `b` → `xmm1`).
    3. Emit code to move these constants into their allocated registers.

#### 3.2. **Register Allocation**
Registers are allocated dynamically for `WHERE` variables, intermediate results, and computational steps.
- Allocation Strategy:
    1. Use free registers (`xmm0` – `xmm15` for floating-point).
    2. If all registers are occupied, assign the variable to a memory spill slot.

#### 3.3. **Expression Evaluation**
- Recursive algorithm walks through the AST and emits dense machine code to compute the value of expressions.

### Sample Workflow
#### Input Example:
``` forth
LET (x) = FN(a, b) = a * b + 5 WHERE a = 4.0 WHERE b = 3.0;
```
#### Execution Steps:
1. **Parsing**:
    - Identify variables: `a`, `b`.
    - Build AST:
``` 
     (+)
      ├── (*)
      │   ├── a
      │   └── b
      └── 5
```
1. **Preallocate Memory**:
    - Assign memory/registers for `a` and `b`.
    - Reserve a register for the result.

2. **Code Generation**:
    - Assembly instructions:
``` 
     movaps xmm0, [a]
     movaps xmm1, [b]
     mulps xmm0, xmm1
     addps xmm0, [5]
     movaps [x], xmm0
```
1. **Execution**:
    - JIT compile and execute the assembly code.
    - Output the result: `17.0`.

To see the generated code SET LOGGING ON.

The code is commented by the code generator.

e.g. 

```forth
SET LOGGING ON
: t3 LET (x) = FN(a, b) = a + b WHERE a = 5.0 WHERE b = 3.0; 
Compiling LET statement:  : T3 LET (X) = FN(A, B) = A + B WHERE A = 5.0 WHERE B = 3.0; 
Word: T2
  State: EXECUTABLE
  Type: WORD
  Data Pointer: 0x0
  Data Size: 0
  word_id: 75
  vocab_id: 1
  ID: 100000075
  Previous Word: 0x6000007aba80
  Executable: 1
  Generator: 0
  Immediate Interpreter: 0
Offsets and Alignment Check for ForthDictionaryEntry structure:
-------------------------------------------------------------------------
Memory Address of Entry: 0x6000007abb00 16 byte Aligned: Yes
-------------------------------------------------------------------------
                         Field Offset (bytes)    16-Byte Aligned?
-------------------------------------------------------------------------
                      previous              0                 Yes
                       word_id             16                 Yes
                      vocab_id             20                  No
                            id             16                 Yes
                         state             32                 Yes
                    executable             48                 Yes
         immediate_interpreter             80                 Yes
                     generator             64                 Yes
                          data             96                 Yes
            immediate_compiler            112                 Yes
                          type            120                  No
-------------------------------------------------------------------------
input: let (x) = fn(a, b) = a + b where a = 5.0 where b = 3.0; 
LetStatement:
  Output Vars: x 
  Input Params: a b 
  Expressions:
    ExprType=3, value='+'
      ExprType=1, value='a'
      ExprType=1, value='b'
  Where Clauses:
    Where: a =
      ExprType=0, value='5.0'
    Where: b =
      ExprType=0, value='3.0'
found free register 0
allocated register for variable a
allocated register for input var a
found free register 1
allocated register for variable b
allocated register for input var b
found free register 2
allocated register for variable x
allocated register for output var x
allocated register for where var a
allocated register for where var b
generating unique temp name _tmp11
found free register 3
allocated register for variable _tmp11
generating unique temp name _tmp12
found free register 4
allocated register for variable _tmp12
generating unique temp name _tmp13
found free register 5
allocated register for variable _tmp13
generating unique temp name _tmp14
found free register 6
allocated register for variable _tmp14
generating unique temp name _tmp15
found free register 7
allocated register for variable _tmp15

Ok 
> align 16
; -- enter function: T3 
; -- enter_function
L0:
L2:
; ----- RBP is set to dictionary entry
mov rax, 0x6000007ABB00                     ; 48B800BB7A0000600000
mov rbp, rax                                ; 4889C5
mov rdi, 0                                  ; 48C7C700000000
; Load variable from FORTH stack: a
movq xmm0, r13                              ; 66490F6EC5
; -- DROP 
mov r13, r12                                ; 4D89E5
mov r12, [r15]                              ; 4D8B27
add r15, 8                                  ; 4983C708
; Load variable from FORTH stack: b
movq xmm1, r13                              ; 66490F6ECD
; -- DROP 
mov r13, r12                                ; 4D89E5
mov r12, [r15]                              ; 4D8B27
add r15, 8                                  ; 4983C708
; WHERE: a = (...)
; Loading literal 5.0 into register _tmp14
; Loading immediate literal 5.0 (value = 5.000000)
mov rax, 0x4014000000000000                 ; 48B80000000000001440
movq xmm6, rax                              ; 66480F6EF0
movaps xmm0, xmm6                           ; 0F28C6
; WHERE: b = (...)
; Loading literal 3.0 into register _tmp15
; Loading immediate literal 3.0 (value = 3.000000)
mov rax, 0x4008000000000000                 ; 48B80000000000000840
movq xmm7, rax                              ; 66480F6EF8
movaps xmm1, xmm7                           ; 0F28CF
; === Main Expression(s) ===
; Emit expression 0 for output var 'x'
; Copy variable 'a' -> expression reg '_tmp12'
movaps xmm4, xmm0                           ; 0F28E0
; Copy variable 'b' -> expression reg '_tmp13'
movaps xmm5, xmm1                           ; 0F28E9
movaps xmm3, xmm4                           ; 0F28DC
addsd xmm3, xmm5                            ; F20F58DD
; Pushing result of 'x' onto stack
sub r15, 8                                  ; 4983EF08
mov [r15], r12                              ; 4D8927
mov r12, r13                                ; 4D89EC
movq r13, xmm3                              ; 66490F7EDD
; === End of Main Expression(s) ===
; -- exit_label
L1:
ret                                         ; C3
; end of -- T3 --


```


## Thread Safety

Within a thread each LET statement has complete access to all the floating point registers, and to a pool of memory.

All the floating point registers may be clobbered by a LET statement.

The floating-point evaluator is thread-safe due to:
1. **Thread-Local Memory**:
    - Global memory is defined as `thread_local`.
    - Each thread gets its independent memory buffer.

2. **Instance Isolation**:
    - Each `LetCodeGenerator` instance owns its own register map and variable allocations.

## Common Error Scenarios
1. **Memory Overflows**:
    - If an expression or number of variables exceeds `GLOBAL_MEMORY_SIZE`, a runtime exception is thrown.
    - Example error: `"Thread-local memory pool overflow"`

2. **Unresolved Variables**:
    - If a variable in the expression is not defined in `WHERE` clauses, a `std::out_of_range` error is thrown.
    - Example error: `"unordered_map::at: key not found"`

3. **Invalid LET Syntax**:
    - If the `LET` statement is malformed, the parser throws a syntax error.



## Example
```forth

: T1 LET (z_next_re, z_next_im, mag) = FN(z_re, z_im, x, y ) = 
     z_next_re, 
     z_next_im, 
     z_sq
    WHERE z_next_re = (z_re * z_re) - (z_im * z_im) + x
    WHERE z_next_im = (2 * z_re * z_im) + y 
    WHERE z_sq = (z_next_re * z_next_re) + (z_next_im * z_next_im) ;
    
    
    : T LET (re, im) = FN(r, i, x, y ) = re * re - im * im + x, 2 * re * im + y ;
            

-0.75 0. -0.75 0. T f. f. f. f. 

0. 0. -0.7 0.3  T f. f. f. f.


: F0 let (result) = fn() =
    result
  where result = 2 + 3;
  
 : F1 
 let (result) = fn() =
    result
  where result = 2 * 3 + 4;
  
  
  : F2 
   let (result) = fn() =
    result
  where result = (2 + 3) * (4 - 1); 
  
  : F3 
    let (result) = fn(x) =
    result
  where result = x * 2 + 3;
  
  : F4 
    let (a, b, c) = fn(x) =
    a,
    b,
    c
  where a = x + 1
  where b = a * 2
  where c = b + 3; 
  
  
  : F5
    let (result1, result2) = fn(x, y) =
    result1,
    result2
  where temp = x + y
  where result1 = temp * temp
  where result2 = temp + y;
  
  
  : F6 let (y) = fn(x) = result
        where result = 2 + 3 * 4;
  

: mandelbrot ( x y max_iter -- result )
    0 0             \ Initialize z_re = 0, z_im = 0
    0               \ Start with iteration count = 0
    BEGIN
    dup max_iter <                \ Check if iteration count < max_iter
    swap                            \ Bring magnitude back to TOS
    4.0 >                          \ Check if magnitude > 4.0 (divergence)
    or                             \ Stop if either max_iter is reached or magnitude > 4
    WHILE
    ( x y z_re z_im iteration_count ) \ Stack state before calling LET
    mandelbrot_step                   \ Call the LET-based floating-point math
    drop drop                         \ Remove z_next_re, z_next_im from TOS
    swap                             \ Bring magnitude to TOS
    1+                               \ Increment iteration count
    REPEAT
    drop                              \ Drop the unused iteration count
;

```

## Conclusion
The floating-point expression evaluator is an advanced feature designed for high-performance numerical computation. Its modular architecture ensures robust handling of dynamic expressions, optimized code generation, and precise floating-point arithmetic. With its inherent thread-safety and support for JIT compilation, it serves as a powerful tool for floating-point operations in the `ForthJIT` project.
