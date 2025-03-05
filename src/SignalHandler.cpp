#include "SignalHandler.h"
#include <csignal>
#include <csetjmp>
#include <cstdio>

// Public method to raise an exception
void SignalHandler::raise(int eno) {
    // Determine the number of available exceptions
    size_t num_exceptions = sizeof(exception_messages) / sizeof(exception_messages[0]);

    // Ensure the error number is within bounds
    if (eno < 0 || static_cast<size_t>(eno) >= num_exceptions) {
        eno = 0; // Default to "Unknown error" if out of range
    }

    // Print the error message
    fprintf(stderr, "FORTH RUNTIME ERROR: %s (Error %d)\n", exception_messages[eno], eno);

    // Instead of exiting, jump back to `quit_env`'s saved state
    longjmp(quit_env, 1);
}

// Public: Register system signal handlers
void SignalHandler::register_signal_handlers() {
    // Register our custom signal handling callback
    signal(SIGINT, SignalHandler::handle_signal);  // Ctrl+C
    signal(SIGFPE, SignalHandler::handle_signal);  // Floating-point exceptions (e.g., division by zero)
    signal(SIGSEGV, SignalHandler::handle_signal); // Segmentation fault
}


// Static: General signal handling logic
void SignalHandler::handle_signal(int signal_number) {
    // Map the signal number to an error code
    int error_code = 0; // Default to "Unknown error"
    switch (signal_number) {
        case SIGINT:
            error_code = 9;  // "Break on CTRL/C"
        break;
        case SIGFPE:
            error_code = 4;  // "Division by zero"
        break;
        case SIGSEGV:
            error_code = 15; // "Invalid memory access (SIGSEGV)"
        break;
        default:
            error_code = 0;  // "Unknown error"
        break;
    }

    // Raise the corresponding error using the singleton instance
    SignalHandler::instance().raise(error_code);
}

// Public method to access the jump buffer
jmp_buf &SignalHandler::get_jump_buffer() {
    return quit_env;
}