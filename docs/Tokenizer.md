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
    L -->|:| M[TOKEN_COMPILING]
    L -->|;| N[TOKEN_INTERPRETING]
    L -->|"("| O[TOKEN_BEGINCOMMENT, inComment=True]
    L -->|")"| P[TOKEN_ENDCOMMENT, inComment=False]
    L -->|is_float()| Q[TOKEN_FLOAT, Convert to float]
    L -->|is_number()| R[TOKEN_NUMBER, Convert to int]
    L -->|Unknown| S[TOKEN_UNKNOWN]

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
