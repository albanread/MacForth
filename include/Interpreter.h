#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "Tokenizer.h"
#include "Singleton.h"
#include <string>
#include <deque>

class Interpreter : public Singleton<Interpreter> {
    // Make the Singleton<Interpreter> a friend to allow access to private constructor
    friend class Singleton<Interpreter>;

public:
    // Main function to execute Forth code
    void execute(const std::string &input);

private:
    // Private constructor and destructor (required for singleton)
    Interpreter() = default;
    ~Interpreter() = default;

    // Helper methods for token processing
    void handle_comment(ForthToken &first, std::deque<ForthToken> &tokens);
    void handle_word(std::deque<ForthToken> &tokens);
    void handle_compiling(std::deque<ForthToken> &tokens);
    void handle_number(const ForthToken &first, std::deque<ForthToken> &tokens);
    void handle_float(const ForthToken &first, std::deque<ForthToken> &tokens);
    void handle_unknown(const ForthToken &first);

    void raise_error(int code, const std::string &message);
};

#endif // INTERPRETER_H