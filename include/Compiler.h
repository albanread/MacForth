#ifndef COMPILER_H
#define COMPILER_H

#include <deque>
#include <string>
#include "Singleton.h"
#include "Tokenizer.h"

class Compiler : public Singleton<Compiler> {
    // The Singleton template will manage instance creation
    friend class Singleton<Compiler>;

public:
    void compile_let(const std::string &input);

    // Main compile entry point
    void compile_words(std::deque<ForthToken> &input_tokens);

private:
    // Constructor and destructor are private to enforce Singleton behavior
    Compiler() = default;
    ~Compiler() = default;

    // Utility and helper methods
    void validate_compiler_state(std::deque<ForthToken> &tokens);
    std::string extract_word_name(std::deque<ForthToken> &tokens);

    // Token processing
    void process_token(const ForthToken &token, std::deque<ForthToken> &tokens, std::string &word_name);
    void compile_token_number(const ForthToken &token);
    void compile_token_float(const ForthToken &token);
    void compile_token_word(const ForthToken &token, std::deque<ForthToken> &tokens, std::string &word_name);
    void compile_token_optimized(const ForthToken &token, std::deque<ForthToken> &tokens);
};

#endif // COMPILER_H