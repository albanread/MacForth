```mermaid
graph TD;
    A[Tokenizer::tokenize_forth] -->|Start| B[Initialize cursor, clear tokens]
    B --> C[Loop: Read next token]
    
    C --> D{End of Input?}
    D -->|Yes| E[TOKEN_END, Stop]
    D -->|No| F[Tokenizer::get_next_token]
    
    F -->|Skip Whitespace| G[skip_whitespace]
    F -->|Parse Word| H[Read characters until space]
    
    H --> I{In Symbol Table?}
    I -->|Yes| J[TOKEN_WORD, set word_id]
    I -->|No| K{Special Token?}
    
    K -->|Yes| L[Check Type]
    L --> M[TOKEN_COMPILING if `:`]
    L --> N[TOKEN_INTERPRETING if `;`]
    L --> O[TOKEN_BEGINCOMMENT if `(`]
    L --> P[TOKEN_ENDCOMMENT if `)`]
    L --> Q[TOKEN_FLOAT if float detected]
    L --> R[TOKEN_NUMBER if integer detected]
    L --> S[TOKEN_UNKNOWN otherwise]

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
