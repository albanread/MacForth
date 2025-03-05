# ADR 008: Forth Dictionary Entry Design and Impact on CREATE/DOES>

## Context

In this Forth implementation, each dictionary entry has three distinct execution behaviors, as represented in the `ForthDictionaryEntry` structure:

1. **Executable Behavior (`executable`)** – This is the standard execution function when the word is invoked in interpretation mode.
2. **Immediate Behavior (`immediate_interpreter`)** – This is executed immediately when the word is encountered in compilation mode (for IMMEDIATE words).
3. **Generator Behavior (`generator`)** – This defines how the word compiles itself into a new definition (for defining custom compilation behavior, similar to state-smart words).

Additionally, the dictionary structure includes metadata such as:
- The word's name and vocabulary (stored as symbol table IDs)
- A pointer to allocated data space (`data`)
- A linked list structure (`previous`) for dictionary threading
- A `ForthState` enum to determine whether the word is `EXECUTABLE`, `IMMEDIATE`, or `GENERATOR`

This design allows for greater flexibility by distinguishing words that:
- Execute normally (`executable`)
- Execute during compilation (`immediate_interpreter`)
- Generate compile-time code (`generator`)

---

## Decision

We will integrate this design with `CREATE` and `DOES>`, ensuring that the word creation process accommodates all three execution behaviors.

### CREATE Behavior
- `CREATE` will allocate a new dictionary entry with an empty executable, `immediate_interpreter`, and `generator`.
- The default behavior of a `CREATE`-defined word is to push its data field address onto the stack.
- The data field will be allocated via `AllotData()`, ensuring it has a unique storage location.

### DOES> Behavior
- `DOES>` modifies the executable behavior of the most recently defined word.
- When `DOES>` is applied, the executable function pointer of the latest `ForthDictionaryEntry` is set to the newly defined behavior.
- The original data field address is still pushed onto the stack before execution, preserving classic Forth semantics.

---

## Impact on IMMEDIATE and GENERATOR Words

- Since `DOES>` modifies the executable function, it does not directly affect `immediate_interpreter` or `generator`.
- A separate `GENERATOR>` word could be introduced to modify the generator function pointer, allowing words to customize their behavior during compilation.
- Similarly, `IMMEDIATE>` could modify `immediate_interpreter`, giving IMMEDIATE words custom behavior in compilation mode.

---

## Consequences

- This design ensures `CREATE` remains flexible, supporting both execution-time behavior (`DOES>`) and compile-time behavior (`GENERATOR>`).
- `DOES>` does not interfere with `immediate_interpreter` or `generator`, maintaining clean separation between execution and compilation phases.
- `GENERATOR>` and `IMMEDIATE>` can be introduced for finer control over how words behave in different Forth states.
- This approach results in a powerful, extensible Forth dictionary system that maintains classic Forth semantics while introducing additional flexibility for defining compilation behaviors.