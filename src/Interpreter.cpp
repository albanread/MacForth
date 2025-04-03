#include "Interpreter.h"
#include "Tokenizer.h"
#include "ForthDictionary.h"
#include "CodeGenerator.h"
#include "Compiler.h"
#include <deque>
#include <iostream>

#include "SignalHandler.h"

// Helper: Raise error with additional message
void Interpreter::raise_error(int code, const std::string &message) {
    std::cerr << "Interpreter Error (" << code << "): " << message << std::endl;
    SignalHandler::instance().raise(code);
}

// Helper: Handle comments
void Interpreter::handle_comment(ForthToken &first, std::deque<ForthToken> &tokens) {
    while (first.type != TokenType::TOKEN_ENDCOMMENT) {
        tokens.pop_front(); // Remove current token
        if (tokens.empty()) {
            raise_error(1, "Unexpected end of comment -- ')'");
            return;
        }
        first = tokens.front(); // Update current token
    }
    tokens.pop_front(); // Remove the ending TOKEN_ENDCOMMENT
}

// Helper: Handle TOKEN_WORD logic
void Interpreter::handle_word(std::deque<ForthToken> &tokens) {
    if (tokens.empty())
        return;

    const ForthDictionary &dict = ForthDictionary::instance();
    const ForthToken &first = tokens.front();
    tokens.pop_front(); // Efficiently remove the first token

    if (first.type == TokenType::TOKEN_WORD || first.type == TokenType::TOKEN_VARIABLE) {
        auto word_found = dict.findWordByToken(first);
        if (word_found == nullptr) {
            raise_error(5, "Word not found: " + first.value);
            return;
        }

        if (word_found->executable) {
            word_found->executable();
        } else if (word_found->immediate_interpreter && word_found->type != ForthWordType::MACRO) {
            word_found->immediate_interpreter(tokens);
        }
    }
}

// Helper: Handle TOKEN_COMPILING
void Interpreter::handle_compiling(std::deque<ForthToken> &tokens) {
    Compiler::instance().compile_words(tokens);
}

// Helper: Handle TOKEN_NUMBER
void Interpreter::handle_number(const ForthToken &first, std::deque<ForthToken> &tokens) {
    cpush(first.int_value); // Push the integer to the stack
    tokens.pop_front(); // Remove the processed token
}

// Helper: Handle TOKEN_FLOAT
void Interpreter::handle_float(const ForthToken &first, std::deque<ForthToken> &tokens) {
    cfpush(first.float_value); // Push the float to the stack
    tokens.pop_front(); // Remove the processed token
}

// Helper: Handle unknown or unprocessed tokens
void Interpreter::handle_unknown(const ForthToken &first) {
    std::cerr << "Unknown token type: " << first.value << std::endl;
}

// Main entry point for interpreting Forth code
void Interpreter::execute(const std::string &input) {
    // Check if the input contains "LET"
    if (input.find("LET") != std::string::npos) {
        Compiler::instance().compile_let(input);
        return;
    }


    std::deque<ForthToken> tokens;

    // Tokenize the input into Forth tokens
    Tokenizer::instance().tokenize_forth(input, tokens);

    while (!tokens.empty()) {
        ForthToken &first = tokens.front();

        // Use a switch structure for token processing
        switch (first.type) {
            case TokenType::TOKEN_BEGINCOMMENT:
                handle_comment(first, tokens);
                break;

            case TokenType::TOKEN_WORD:
            case TokenType::TOKEN_VARIABLE:
                handle_word(tokens);
                break;

            case TokenType::TOKEN_COMPILING:
                handle_compiling(tokens);
                break;

            case TokenType::TOKEN_NUMBER:
                handle_number(first, tokens);
                break;

            case TokenType::TOKEN_FLOAT:
                handle_float(first, tokens);
                break;

            case TokenType::TOKEN_END:
                tokens.pop_front(); // Remove TOKEN_END and exit
                return;

            default:
                handle_unknown(first);
                tokens.pop_front(); // Remove the unhandled token
                break;
        }
    }
}
