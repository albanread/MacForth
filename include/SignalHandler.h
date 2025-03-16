#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include "Singleton.h"
#include <setjmp.h>
#include <stdio.h>

class SignalHandler : public Singleton<SignalHandler> {
    friend class Singleton<SignalHandler>;

public:
    // Public method to raise an error
    void raise(int eno);

    // Public method to get the jump buffer
    jmp_buf &get_jump_buffer();

    void register_signal_handlers();

private:
    // Constructor (private to enforce singleton)
    SignalHandler() = default;

    // Destructor
    ~SignalHandler() = default;

    // Static array of exception messages
    static constexpr const char *exception_messages[] = {
        "Unknown error",                                     // 0
        "Stack underflow",                                   // 1
        "Stack overflow",                                    // 2
        "Invalid memory access",                             // 3
        "Division by zero",                                  // 4
        "Invalid word",                                      // 5
        "Invalid execution token",                           // 6
        "Undefined behavior",                                // 7
        "ERROR: EXEC Attempted to execute NULL XT",          // 8
        "Break on CTRL/C",                                   // 9
        "Assembler Not initialized",                        // 10
        "String was expected by .\"",                       // 11
        "Error finalizing the JIT-compiled function",        // 12
        "Defer word has no action set.",                    // 13
        "Word not found.",                                   // 14
        "SIGSEGV: consider if it is still safe to proceed.", // 15
        "Compiler: ':' or ']' expected to start compilation.", // 16
        "Compiler: new name expected.",                     // 17
        "DEFINITIONS: needed a preceding vocabulary.",      // 18
        "Unhandled token type encountered",                  // 19
        "Failed to initialize CodeHolder with JIT environment.", // 20
        "Label Manager error.", // 21
        "LET statement generator error." , // 22
        "LET statement Lexer error.", // 23
        "LET statement Parser error.", // 24
        "Register Tracker error" // 25
    };

    // Jump buffer for longjmp
    jmp_buf quit_env;

    // Static signal handler callbacks
    static void handle_signal(int signal_number); // General signal handler
};

#endif // SIGNAL_HANDLER_H