+-------------------------------+
|  ForthDictionaryEntry         |
+-------------------------------+
| *previous (linked list)       |  <-- Points to previous entry
| word_id (Symbol Table ID)     |
| vocab_id (Vocabulary ID)      |
| state (ForthState)            |
| *executable (Function Ptr)    |  <-- Code execution
| *generator (Function Ptr)     |  <-- Custom word creation
| *immediate_interpreter (Ptr)  |  <-- Immediate word behavior
| *immediate_compiler (Ptr)     |  <-- Compile-time behavior
| *data (Heap Ptr)              |  <-- Associated data storage
| *firstWordInVocabulary        |  <-- Linked list by vocabulary
+-------------------------------+

        Dictionary Organization:

        +-----------------------------+
        | dictionaryLists (Array)      |  <-- Indexed by word length (0..16)
        | [ * ][ * ][ * ][ * ] ...     |
        +-----------------------------+

        +-----------------------------+
        | wordOrder (Vector)           |  <-- Ordered list of all words added
        | [ * ][ * ][ * ][ * ] ...     |
        +-----------------------------+

    ┌───────────────────────────────────────────────┐
    │  Symbol Table (Stores Word & Vocabulary Names) │
    ├──────────────┬───────────────────────────────┤
    │ word_id      │  "DUP"                         │
    │ word_id      │  "SWAP"                        │
    │ vocab_id     │  "CORE"                        │
    │ vocab_id     │  "MATH"                        │
    └──────────────┴───────────────────────────────┘
