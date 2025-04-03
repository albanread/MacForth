#include <iostream>
#include <csignal>
#include <csetjmp>
#include "Quit.h"
#include <Interpreter.h>
#include "LineReader.h"
#include "SignalHandler.h"
#include "Settings.h"

// Function to fetch registers for debugging (example placeholders)
uint64_t fetchR15();

uint64_t fetchR13();

uint64_t fetchR12();

uint64_t fetch3rd();

uint64_t fetch4th();

extern uintptr_t stack_top;
extern uintptr_t stack_base;


void display_stack_status() {
    const auto depth = static_cast<int64_t>((stack_top - fetchR15() > 0) ? ((stack_top - fetchR15()) / 8) : 0);
    std::cout << std::endl << "Ok ";
    if (print_stack) {
        std::cout <<
                "DEPTH[" << depth << "] "
                "[TOP]=[" << static_cast<int64_t>(fetchR13()) << "] "
                << " [2nd]=[" << static_cast<int64_t>(fetchR12()) << "] "
                << " [3rd]=[" << static_cast<int64_t>(fetch3rd()) << "] "
                << " [4th]=[" << static_cast<int64_t>(fetch4th()) << "] ";
    }
    std::cout << std::endl;
}

bool containsColonSpace(const std::string &input) {
    // Check if ": " is found in the string
    return input.find(": ") != std::string::npos;
}

bool containsSemiColon(const std::string &input) {
    // Check if " ;" is found in the string
    return input.find(';') != std::string::npos;
}

void to_uppercase(std::string &str) {
    bool in_quotes = false; // Track if we're inside quotes
    char current_quote = '\0'; // Tracks the type of quote we are inside (' or ")

    for (size_t i = 0; i < str.size(); ++i) {
        char &c = str[i];

        // Check if the current character is a quote
        if ((c == '\'' || c == '"') && (i == 0 || str[i - 1] != '\\')) {
            if (!in_quotes) {
                // Entering a quoted section
                in_quotes = true;
                current_quote = c;
            } else if (c == current_quote) {
                // Exiting the quoted section
                in_quotes = false;
                current_quote = '\0';
            }
            continue; // Do not change quotes themselves
        }

        // Only convert to uppercase if outside quotes
        if (!in_quotes) {
            c = std::toupper(static_cast<unsigned char>(c));
        }
    }
}

inline void interactive_terminal() {
    std::string input;
    std::string accumulated_input;
    bool compiling = false;

    // std::cout << "ForthJIT " << std::endl;
    display_stack_status();

    LineReader::initialize();

    // The infinite terminal loop
    while (true) {
        std::cout << (compiling ? "] " : "> ") << std::flush;

        input = LineReader::readLine();

        // Exit condition
        if (input == "BYE" || input == "bye") {
            LineReader::finalize();
            exit(0);
        }

        // Discard comments (strip everything after `\`)
        size_t comment_pos = input.find("\\");
        if (comment_pos != std::string::npos) {
            input = input.substr(0, comment_pos); // Keep everything before the `\`
        }

        // Ignore empty input after stripping comments
        if (input.empty()) {
            continue;
        }

        // Compilation mode toggles (based on colon/semicolon)
        if (containsColonSpace(input)) compiling = true;
        if (containsSemiColon(input)) compiling = false;

        accumulated_input += " " + input; // Accumulate input lines
        if (!compiling) {
            to_uppercase(accumulated_input);
            Interpreter::instance().execute(accumulated_input);
            accumulated_input.clear();
            display_stack_status();
        }
    }
}


// The Quit function with setjmp for recovery
void Quit() {
    SignalHandler::instance().register_signal_handlers();


    // Main program loop
    while (true) {
        // Save the current execution state for recovery
        if (setjmp(SignalHandler::instance().get_jump_buffer()) == 0) {
            // Run the main interactive interpreter loop
            interactive_terminal();
        } else {
            // If an exception is raised (via longjmp), handle it here
            // std::cout << "Recovered from a runtime error. Restarting interpreter." << std::endl;
        }
    }
}
