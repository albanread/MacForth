#include "Compiler.h"
#include <ForthDictionary.h>
#include <iostream>
#include "ControlFlow.h"
#include "CodeGenerator.h"
#include  "LetCodeGenerator.h"
#include "Tokenizer.h"
#include "Optimizer.h"

#include "SignalHandler.h"
#include "Settings.h"



void Compiler::compile_let(const std::string &input) {

    if (jitLogging) std::cout << "Compiling LET statement: " << input << std::endl;

    size_t colonPos = input.find(": ");
    if (colonPos == std::string::npos) {
        std::cerr << ("Syntax error: Expected ': <function_name>' in LET statement");
    }

    // Extract the function name, which is the word immediately after ": "
    size_t nameStart = colonPos + 2; // Skip past ": "
    size_t nameEnd = input.find(' ', nameStart); // Find the next space to isolate the name

    // Check if a name exists after ": "
    if (nameStart == std::string::npos || nameStart >= input.size()) {
        std::cerr << ("Syntax error: Missing function name after ':'");
    }

    std::string functionName = input.substr(nameStart, nameEnd - nameStart);

    // Validate the extracted name if necessary
    if (functionName.empty()) {
         std::cerr << ("Syntax error: Function name cannot be empty");
    }

    // step 3
    size_t letPos = input.find("LET");
    if (letPos == std::string::npos) {
        std::cerr << "Syntax error: Expected 'LET' in the statement";
        return; // Exit if "LET" is not found
    }


    // Step 2: Remove everything before "LET"
    std::string letString = input.substr(letPos);
    std::transform(letString.begin(), letString.end(), letString.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    code_generator_startFunction(functionName);

    const auto tokens = tokenize(letString);
    Parser parser(tokens);
    const auto ast = parser.parseLetStatement();

    if ( jitLogging) parser.printAST(ast.get());

    if (functionName.length() > 16) {
        std::cerr << "Function name too long: " << functionName << std::endl;
        functionName = functionName.substr(0, 16);
        std::cout << "Truncated name to:" << functionName << std::endl;
    }


    // some c++ classes used by the generator may raise horrid exceptions.

    try {


        LetCodeGenerator::instance().initialize();

        LetCodeGenerator::instance().generateCode(ast.get());



    } catch (const std::out_of_range &e) {
        // Specific exception for missing keys in an unordered map
        std::cerr << "Out of range exception: " << e.what() << std::endl;

        // Key debugging - Log context of the exception
        std::cerr << "Debugging map error: Possible missing key in unordered_map." << std::endl;
        // generator.printRegisterMaps();


        SignalHandler::instance().raise(22);

    } catch (const std::exception &e) {
        // Handle standard exceptions
        std::cerr << "An error occurred: " << e.what() << std::endl;
        SignalHandler::instance().raise(22);

    } catch (...) {
        // Handle any other types of exceptions
        std::cerr << "An unknown error occurred during code generation." << std::endl;
        SignalHandler::instance().raise(22);
    }



    compile_return();
    const ForthFunction f = code_generator_finalizeFunction(functionName);
    //
    auto &dict = ForthDictionary::instance();
    dict.addCodeWord(functionName, "FORTH",
                    ForthState::EXECUTABLE,
                    ForthWordType::WORD,
                    nullptr,
                    f,
                    nullptr);


}

 
void Compiler::compile_words(std::deque<ForthToken> &input_tokens) {
    // Create a copy of tokens and optimize if necessary
    std::deque<ForthToken> tokens = input_tokens;
    if (optimizer == true) {
        Optimizer::instance().optimize(input_tokens, tokens);
    }
    input_tokens.clear();
    // Tokenizer::instance().print_token_list(tokens);
    // Step 1: Validate the compiler state and token structure
    validate_compiler_state(tokens);

    // Step 2: Extract the word name
    std::string word_name = extract_word_name(tokens);
    // std::cout << "Compiling: " << word_name << std::endl;

    // Step 3: Start code generation for the function
    code_generator_startFunction(word_name);

    // Step 4: Process tokens
    while (!tokens.empty()) {
        const auto &token = tokens.front();

        // Handle end conditions
        if (token.type == TokenType::TOKEN_END || token.type == TokenType::TOKEN_INTERPRETING) {
            break;
        }

        // std::cout << "-- Generating: " << token.value;
        process_token(token, tokens, word_name);
        std::cout << std::endl;

        tokens.pop_front(); // Remove the processed token
    }

    // Step 5: Finalize the function and add it to the dictionary
    compile_return();
    ForthFunction f = code_generator_finalizeFunction(word_name);
    auto &dict = ForthDictionary::instance();
    dict.addCodeWord(word_name, dict.getCurrentVocabularyName(),
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     nullptr,
                     f,
                     nullptr);
}

// Helper Method: Validate Compiler State
void Compiler::validate_compiler_state(std::deque<ForthToken> &tokens) {
    auto token = tokens.front();
    if (token.type != TokenType::TOKEN_COMPILING) {
        SignalHandler::instance().raise(16); // "Colon expected" or other similar error
    }
    tokens.pop_front();
    if (tokens.empty()) {
        SignalHandler::instance().raise(16); // "Colon expected" or other similar error
    }
}

// Helper Method: Extract Word Name
std::string Compiler::extract_word_name(std::deque<ForthToken> &tokens) {
    auto token = tokens.front();
    if (token.type != TokenType::TOKEN_UNKNOWN) {
        SignalHandler::instance().raise(17); // "New name expected"
    }
    tokens.pop_front();
    if (tokens.empty()) {
        SignalHandler::instance().raise(6);
    }
    return token.value; // Extract word name
}

// Helper Method: Process Token
void Compiler::process_token(const ForthToken &token, std::deque<ForthToken> &tokens, std::string &word_name) {
    switch (token.type) {
        case TokenType::TOKEN_NUMBER:
            compile_token_number(token);
            break;

        case TokenType::TOKEN_FLOAT:
            compile_token_float(token);
            break;

        case TokenType::TOKEN_WORD:
        case TokenType::TOKEN_VARIABLE:
            compile_token_word(token, tokens, word_name);
            break;

        case TokenType::TOKEN_OPTIMIZED:
            compile_token_optimized(token, tokens);
            break;

        default:
            std::cerr << "Compiler: Unhandled token type: " << token.value << std::endl;
            SignalHandler::instance().raise(6);
    }
}

// Helper Method: Compile Number Token
void Compiler::compile_token_number(const ForthToken &token) {
    //std::cout << "LITERAL: " << token.int_value;
    compile_pushLiteral(token.int_value);
}

// Helper Method: Compile Float Token
void Compiler::compile_token_float(const ForthToken &token) {
    //std::cout << "FLOAT LITERAL: " << token.float_value;
    compile_pushLiteralFloat(token.float_value);
}

// Helper Method: Compile Word Token
void Compiler::compile_token_word(const ForthToken &token, std::deque<ForthToken> &tokens, [[maybe_unused]] std::string &word_name) {
    //std::cout << "CALL WORD: " << token.value << std::endl;

    auto word_found = ForthDictionary::instance().findWord(token.value.c_str());
    if (word_found == nullptr) {
        std::cerr << "Word not found: " << token.value << std::endl;
        SignalHandler::instance().raise(6);
        return;
    }

    const std::string called_word_name = token.value;

    if (word_found->type == ForthWordType::VARIABLE ) {

        compile_pushVariableAddress(reinterpret_cast<uint64_t>(word_found->data), called_word_name);

    } else if (word_found->generator) {
        // std::cout << "Calling gen: " << token.value << std::endl;
        word_found->generator();
    } else if (word_found->executable) {
        compile_call_forth(word_found->executable, called_word_name);
    } else if (word_found->immediate_compiler) {
        // std::cout << "Calling imm comp: " << token.value << std::endl;
        word_found->immediate_compiler(tokens);
    }
}

// Helper Method: Compile Optimized Token
void Compiler::compile_token_optimized(const ForthToken &token, std::deque<ForthToken> &tokens) {
    // Tokenizer::instance().print_token(token);
    auto word_found = ForthDictionary::instance().findWord(token.optimized_op.c_str());
    if (word_found && word_found->immediate_interpreter) {
        word_found->immediate_interpreter(tokens);
    }
}