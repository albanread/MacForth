## Dictionary
 ```mermaid
graph TD;
    subgraph Dictionary Organization
        A["dictionaryLists (word length 0..16)"]
        B["wordOrder (insertion order)"]
        A -->|Links by word length| C["ForthDictionaryEntry A"]
        A -->|Links by word length| D["ForthDictionaryEntry B"]
        A -->|Links by word length| E["ForthDictionaryEntry C"]
        B -->|Tracks order| C
        B -->|Tracks order| D
        B -->|Tracks order| E
    end
    
    C -->|Previous Entry| F["ForthDictionaryEntry (Previous)"]
    D -->|Previous Entry| C
    E -->|Previous Entry| D
```
```mermaid
graph TD;
    subgraph ForthDictionaryEntry
        A["ForthDictionaryEntry"]
        A -->|word_id, vocab_id| B["Symbol Table"]
        A -->|executable| C["Executable Function"]
        A -->|generator| D["Generator Function"]
        A -->|immediate_interpreter| E["Immediate Interpreter"]
        A -->|immediate_compiler| F["Immediate Compiler"]
        A -->|data| G["Heap Data"]
    end

    B -->|Maps IDs to Names| H["'DUP', 'SWAP'"]
```
