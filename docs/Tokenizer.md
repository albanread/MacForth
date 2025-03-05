```mermaid
graph TD;
    A[Tokenizer::tokenize_forth] -->|Start| B[Initialize cursor, clear tokens]
    B --> C[Loop: Read next token]
    
    C --> D{End of Input?}
    D -->|Yes| E[TOKEN_END, Stop]
    D -->|No| F[Tokenizer::get_next_token]
    
    F -->|Whitespace| G[skip_whitespace]
    F -->|Word Parsing| H[Read characters until space]
    
    H --> I{Word exists in SymbolTable?}
    I -->|Yes| J[TOKEN_WORD, set word_id]
    I -->|No| K{Special Token?}
    
    K -->|Yes| L{Check Token Type}
    L -->|Colon `:`| M[TOKEN_COMPILING]
    L -->|Semicolon `;`| N[TOKEN_INTERPRETING]
    L -->|Opening `(`| O[TOKEN_BEGINCOMMENT, inComment=True]
    L -->|Closing `)`| P[TOKEN_ENDCOMMENT, inComment=False]
    L -->|Float detected| Q[TOKEN_FLOAT, Convert to float]
    L -->|Number detected| R[TOKEN_NUMBER, Convert to int]
    L -->|Unknown token| S[TOKEN_UNKNOWN]

    J --> U[Push Token to List]
    M --> U
    N --> U
    O --> U
    P --> U
    Q --> U
    R --> U
    S --> U

    U --> C  %% Loop back to process the next token
    
    E --> T[Return Token List]
```
