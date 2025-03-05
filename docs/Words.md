
## Commands Documentation

There are some commands for viewing and changing settings that use
the SET and SHOW commands.

## SET

#### SET OPTIMIZE ON|OFF 

Enables or disables the optimizer.

The optimizer applies some simple optimizations by analyzing 
the FORTH words the user is creating and substituting more efficient 
code when possible.

#### SET LOGGING ON|OFF 

Enables or disables logging.

With logging ON the generated machine code is displayed when you 
compile words.

The idea is to organize the non-compilable configuration setting words in one place.

## SHOW

#### SHOW CHAIN 

Shows the word chain in the dictionary

#### SHOW ALLOT

Shows the dynamic data allocated by ALLOT to words in the dictionary.

The idea is to organize the non compilable introspection words in one place.


## Forth Words Documentation

This document describes the implemented Forth words, detailing their behavior and usage in the context of the current system.

### **Syntax:**
``` forth
VARIABLE <name>
```
### **Details:**
- **Memory Allocation**: Automatically allots 8 bytes on the heap.
- **Address**: Returns the address of the variable for access.
- **Access Operators**:
    - **`@`**: Read the 8 byte value stored at the variable's address.
    - **`!`**: Write an 8 byte value to the variable's address.

### **Usage Examples:**
``` forth
VARIABLE myVariable     \ Creates a variable called myVariable
123 myVariable !        \ Sets the value of myVariable to 123
myVariable @ .          \ Reads the value at myVariable and prints it (outputs 123)
```
## **Word: `ALLOT`**
### **Description:**
`ALLOT` allocates a specified number of bytes on the heap. 
Unlike standard Forth, where `ALLOT` is typically used to allocate dictionary space, in this implementation, 
`ALLOT` directly manages memory in the heap. 

ALLOT takes a size (in bytes) as an argument and allocates that amount of memory _to the last forth word created_ 
the intention is to copy standard FORTH behaviour.

Unlike standard FORTH a words allotment of data can be changed (non-destructively resized) by subsequent ALLOT statements.

### **Syntax:**
``` forth
<number> ALLOT
```
### **Details:**
- Takes a single argument (the number of bytes to allocate).
- Allocates the specified number of bytes on the heap.
- Returns the address of the allocated memory.

### **Usage Example:**
``` forth
64 ALLOT constant buffer   \ Allocates 64 bytes on the heap and stores its address in buffer
buffer .                   \ Prints the address of the allocated memory
```
## **Word: `SHOW ALLOT`**
### **Description:**
`SHOW ALLOT` lists the current heap allocations. It displays information about all active allotments on the heap, including any memory allocated by `ALLOT` or by `VARIABLE`. This can be useful to debug or explore the memory currently in use.
### **Syntax:**
``` forth
SHOW ALLOT
```
### **Details:**
- Outputs a list of all heap allocations, along with their sizes and addresses.
- Provides a way to inspect the memory usage of the program.

**Output Example**:
``` 
VARIABLE test
WordHeap: Successfully allocated 16 bytes for word: test.

SHOW ALLOT
WordHeap: Current memory allocations:
  Word: test, Size: 16 bytes, Type: Raw Bytes, Element Size: 1 bytes, Rows: 1, Columns: 1, Address: 0x600003090050

64 ALLOT
WordHeap: Reallocation succeeded for word: test, new size: 64 bytes.

SHOW ALLOT
WordHeap: Current memory allocations:
  Word: test, Size: 64 bytes, Type: Raw Bytes, Element Size: 1 bytes, Rows: 1, Columns: 1, Address: 0x600002790000
```

Since memory allocation is not tied to a single dictionary space, we can introduce the word ALLOT> this non standard extension 
allows the allotment of memory to be changed for any word in the dictionary, although it is mainly useful for VARIABLES 
and VALUES that we might want to resize.

```forth

VARIABLE test
WordHeap: Successfully allocated 16 bytes for word: test.

VARIABLE FRED 64 ALLOT
WordHeap: Successfully allocated 16 bytes for word: FRED.
WordHeap: Reallocation succeeded for word: FRED, new size: 64 bytes.

SHOW ALLOT
WordHeap: Current memory allocations:
  Word: FRED, Size: 64 bytes, Type: Raw Bytes, Element Size: 1 bytes, Rows: 1, Columns: 1, Address: 0x600002794040
  Word: test, Size: 64 bytes, Type: Raw Bytes, Element Size: 1 bytes, Rows: 1, Columns: 1, Address: 0x600002790000
 
96 ALLOT> test
WordHeap: Reallocation succeeded for word: test, new size: 96 bytes.

SHOW ALLOT
WordHeap: Current memory allocations:
  Word: FRED, Size: 64 bytes, Type: Raw Bytes, Element Size: 1 bytes, Rows: 1, Columns: 1, Address: 0x600002794040
  Word: test, Size: 96 bytes, Type: Raw Bytes, Element Size: 1 bytes, Rows: 1, Columns: 1, Address: 0x6000016900
```

### Summary of `VARIABLE`, `ALLOT`, and `SHOW ALLOT`

| Word | Description | Example Usage |
| --- | --- | --- |
| `VARIABLE` | Creates a named variable with 8 bytes allocated on the heap. | `VARIABLE myVariable` |
| `ALLOT` | Allocates a specified number of bytes on the heap. | `64 ALLOT` |
| `SHOW ALLOT` | Displays all current heap allocations. |


## Non intrusive Structured Data 

Since allot is allocating memory on the heap, we can also introduce structured memory allotment, ALLOT allots bytes of
storage accessed by the standard fetch and store words, but it can also be convenient to allot data for specific types organized
int specific shapes.



# Other Implementation details

The code generator uses ASMJIT, in this version we target the X86_64 MacOS Intel systems.

The code generated for new words can be inspected, while they are compiled, (SET LOGGING ON) and is intended to be well commented for human and AI review.

To avoid needing state-full words, there are four functions that may be used in each word.










