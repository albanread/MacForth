#include "ForthSystem.h"
#include <iostream>  // For logging initialization (optional)

void ForthSystem::initialize() {
    try {
        // Initialize components in a meaningful order (based on dependencies).

        // Stack and variable subsystems
        DataStack::instance();
        ReturnStack::instance();
        LocalsHandler::instance();
        VariableStorage::instance();
        StringStorage::instance();

        // Storage and signal handling
        SignalHandler::instance();

        LineReader::instance();
        Tokenizer::instance();
        Interpreter::instance();
        Compiler::instance();

        // JIT-related systems
        JitContext::instance();

        // std::cout << "ForthSystem initialized successfully." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error during ForthSystem initialization: " << e.what() << std::endl;
        throw;  // Optional: rethrow to propagate initialization failures
    }
}