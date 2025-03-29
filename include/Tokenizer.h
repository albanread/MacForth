#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <deque>
#include "Singleton.h"
#include <string>


#define MAX_INPUT 1024
#define MAX_WORD_LENGTH 16
#define MAX_TOKEN_LENGTH 1024
#define MAX_TOKENS 1024

typedef enum {
    TOKEN_WORD,         // Normal Forth word
    TOKEN_NUMBER,       // Integer numbers
    TOKEN_FLOAT,        // Floating-point numbers
    TOKEN_STRING,       // Strings
    TOKEN_VARIABLE,     // Variable with data pointer
    TOKEN_UNKNOWN,      // Unidentified tokens
    TOKEN_COMPILING,    // Indicate the compiler is running
    TOKEN_BEGINCOMMENT, // Start of comment
    TOKEN_ENDCOMMENT,   // End of comment
    TOKEN_BEGINLOCALS,  // Start of local variable definition
    TOKEN_ENDLOCALS,    // End of local variable definition
    TOKEN_INTERPRETING, // Indicate the interpreter is running
    TOKEN_END,          // End of token stream

    // New tokens for optimizations
    TOKEN_CONSTANT,     // Constant value
    TOKEN_OPTIMIZED,    // Optimized operation
    TOKEN_OPERATOR,     // Operators (+, -, *, /)
    TOKEN_SHIFT,        // Optimized shift operations (SHL, SHR)
    TOKEN_CALL          // Optimized function calls (Tail call)
} TokenType;

// Token structure with all necessary constructors
struct ForthToken {
    TokenType type = TOKEN_UNKNOWN;
    uint64_t int_value = 0;
    double float_value = 0.0;
    std::string value;

    // Optimization-specific fields
    bool is_optimized = false;
    bool is_immediate = false;
    bool in_comment = false;
    int64_t opt_value = 0;
    TokenType original_type = TOKEN_UNKNOWN;
    std::string optimized_op;
    uint32_t word_id;
    uint32_t word_len;

    // ✅ Default Constructor
    ForthToken() = default;

    // ✅ Constructor for regular tokens
    ForthToken(TokenType t, std::string v = "", int i_val = 0, double f_val = 0.0)
        : type(t), int_value(i_val), float_value(f_val), value(std::move(v)),
          is_optimized(false), opt_value(0), original_type(t), optimized_op("") {}

    // ✅ Constructor for optimized tokens
    ForthToken(TokenType t, std::string op, int64_t opt_val)
        : type(t), int_value(0), float_value(0.0), value(""),
          is_optimized(true), opt_value(opt_val), original_type(t), optimized_op(std::move(op)) {}

    ForthToken(TokenType t, int i_val)
     : type(t), int_value(i_val), float_value(0.0), value(""),
       is_optimized(false), opt_value(0), original_type(t), optimized_op("") {}

    // Constructor for TokenType and std::string
    ForthToken(TokenType t, const std::string& str_val)
        : type(t), int_value(0), float_value(0.0), value(str_val),
          is_optimized(false), opt_value(0), original_type(t), optimized_op("") {}



    // ✅ Copy and move constructors
    ForthToken(const ForthToken&) = default;
    ForthToken(ForthToken&&) noexcept = default;
    ForthToken& operator=(const ForthToken&) = default;
    ForthToken& operator=(ForthToken&&) noexcept = default;


    // ✅ Reset method
    void reset() {
        type = TOKEN_UNKNOWN;
        int_value = 0;
        float_value = 0.0;
        value.clear();
        in_comment = false;
        is_optimized = false;
        opt_value = 0;
        original_type = TOKEN_UNKNOWN;
        optimized_op.clear();
    }
};





class Tokenizer : public Singleton<Tokenizer> {
    friend class Singleton<Tokenizer>;

private:
    bool inComment = false;

public:

    void print_token(const ForthToken &token);

    void print_token_list(const std::deque<ForthToken> &tokens);


    int optimize_constant_operations(const std::deque<ForthToken> &tokens, std::deque<ForthToken> &optimized_tokens);

    ForthToken get_next_token(const char **input);

    int tokenize_forth(const std::string &input, std::deque<ForthToken> &tokens);


    Tokenizer() = default;



};

#endif // TOKENIZER_H
