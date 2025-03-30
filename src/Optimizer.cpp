#include "Optimizer.h"
#include <cstdint>
#include <deque>
#include <SignalHandler.h>
#include <stdexcept>
#include <string>
#include <Tokenizer.h>
#include "SymbolTable.h"
#include "Settings.h"

int optimizations;

int Optimizer::optimize(const std::deque<ForthToken> &tokens,
                        std::deque<ForthToken> &optimized_tokens) {
    optimized_tokens.clear(); // Clear the output deque before optimization
    optimizations = 0;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const ForthToken &current = tokens[i];

        // Optimize peephole cases: specific patterns like "DUP +" or "SWAP DROP"
        if (i + 1 < tokens.size() && optimize_peephole_case(tokens, optimized_tokens, i)) {
            continue; // `optimize_peephole_case` adjusts `i` automatically
        }


        // Optimize constant operations: NUMBER followed by OPERATOR
        if (i + 1 < tokens.size() && current.type == TOKEN_NUMBER && is_arithmetic_operator(tokens[i + 1].value)) {
            if (optimize_constant_operation(tokens, optimized_tokens, i)) {
                i++; // Skip operator as it's already processed
                continue;
            }
        }

        // Optimize literal comparisons: NUMBER followed by <, > or =
        if (i + 1 < tokens.size() && current.type == TOKEN_NUMBER && is_comparison_operator(tokens[i + 1].value)) {

            if (optimize_literal_comparison(tokens, optimized_tokens, i)) {
                i++; // Skip operator as it's already processed

                continue;
            }
        }

        // Copy token if no optimizations were applied
        optimized_tokens.push_back(current);
    }

    // Add TOKEN_END to signal end of optimization
    optimized_tokens.emplace_back(ForthToken{TOKEN_END});
    //Tokenizer::instance().print_token_list(optimized_tokens);
    if (jitLogging)
     std::cout << "Optimizations: " << std::dec << optimizations << std::endl;
    return optimized_tokens.size();
}

// PRIVATE UTILITY FUNCTIONS

bool Optimizer::is_arithmetic_operator(const std::string &op) {
    return (op == "+" || op == "-" || op == "*" || op == "/");
}

bool Optimizer::is_comparison_operator(const std::string &op) {
    return (op == "<" || op == ">" || op == "=");
}

bool Optimizer::optimize_constant_operation(const std::deque<ForthToken> &tokens,
                                            std::deque<ForthToken> &optimized_tokens, size_t index) {
    const ForthToken &number = tokens[index];
    const ForthToken &op = tokens[index + 1];

    ForthToken temp;
    temp.type = TOKEN_OPTIMIZED;
    temp.int_value = number.int_value;

    if (op.value == "+" || op.value == "-") {
        temp.optimized_op = (op.value == "+") ? "ADD_IMM" : "SUB_IMM";
    } else if (op.value == "*") {
        if (number.int_value == 1) return false; // Skip multiplication by 1
        if (is_power_of_two(number.int_value)) {
            temp.optimized_op = "SHL_IMM"; // Optimized for power of 2
            temp.int_value = __builtin_ctz(number.int_value); // Number of trailing zeroes (log2)
        } else {
            temp.optimized_op = "MUL_IMM";
        }
    } else if (op.value == "/") {
        if (number.int_value == 0) throw std::runtime_error("Division by zero detected!");
        if (number.int_value == 1) return false; // Skip division by 1
        if (is_power_of_two(number.int_value)) {
            temp.optimized_op = "SHR_IMM"; // Optimized for power of 2
            temp.int_value = __builtin_ctz(number.int_value);
        } else {
            temp.optimized_op = "DIV_IMM";
        }

    }
    optimizations++;
    set_common_fields(temp);
    optimized_tokens.emplace_back(temp);
    return true;
}

bool Optimizer::optimize_literal_comparison(const std::deque<ForthToken> &tokens,
                                            std::deque<ForthToken> &optimized_tokens, size_t index) {
    if (index + 1 >= tokens.size()) {
        std::cerr << ("Insufficient tokens for literal comparison optimization");
        SignalHandler::instance().raise(5);
    }

    const ForthToken &number = tokens[index];
    const ForthToken &op = tokens[index + 1];

    if (op.value != "<" && op.value != ">" && op.value != "=") {
        std::cerr <<  ("Unexpected operator during literal comparison optimization");
        SignalHandler::instance().raise(5);
    }

    ForthToken temp;
    temp.type = TOKEN_OPTIMIZED; // Mark as optimized
    temp.int_value = number.int_value;

    if (op.value == "<") {
        temp.optimized_op = "CMP_LT_IMM"; // Less than
    } else if (op.value == ">") {
        temp.optimized_op = "CMP_GT_IMM"; // Greater than
    } else if (op.value == "=") {
        temp.optimized_op = "CMP_EQ_IMM"; // Equal
    }
    optimizations++;
    set_common_fields(temp);

    // Add the optimized token to the output
    optimized_tokens.emplace_back(temp);

    // // Handle the token following the operator (if any)
    // if (index + 2 < tokens.size()) {
    //     const ForthToken &next_token = tokens[index + 2];
    //     // Add logic here to queue or process the next token if necessary
    // }


    return true;
}




ForthToken Optimizer::getToken(const std::deque<ForthToken>& tokens, size_t i) {
    return (i < tokens.size()) ? tokens[i] : ForthToken();
}

bool Optimizer::optimize_peephole_case(const std::deque<ForthToken>& tokens,
                                       std::deque<ForthToken>& optimized_tokens, size_t& index) {


    // Lambda to create optimized tokens with less boilerplate
    auto addOptimizedToken = [&](const std::string& optimized_name, int int_value = 0, const std::string& value = "", int word_id = 0) {
        auto token = create_optimized_token(optimized_name);
        token.int_value = int_value;
        token.value = value;
        token.word_id = word_id;
        optimized_tokens.emplace_back(token);
    };

    // Retrieve current token and lookahead tokens
    const ForthToken& current = getToken(tokens, index);
    const ForthToken& next = getToken(tokens, index + 1);
    const ForthToken& third = getToken(tokens, index + 2);
    const ForthToken& fourth = getToken(tokens, index + 3);

    // Pattern-matching optimizations
    if (current.value == "R>" && next.type == TOKEN_NUMBER && third.value == "+" && fourth.value == ">R") {
        addOptimizedToken("INC_R@", next.int_value);
        index += 3; // Skip next 3 tokens
        optimizations++;
        return true;
    }

    if (current.value == "R>" && next.type == TOKEN_NUMBER && third.value == "-" && fourth.value == ">R") {
        addOptimizedToken("DEC_R@", next.int_value);
        index += 3;
        optimizations++;
        return true;
    }

    if (current.type == TOKEN_NUMBER && next.type == TOKEN_VARIABLE && third.value == "!") {
        addOptimizedToken("LIT_VAR_!", current.int_value, next.value);
        index += 2;
        optimizations++;
        return true;
    }

    if (current.value == "R@" && next.value == "C!") {
        addOptimizedToken("R@_C!");
        index += 1;
        optimizations++;
        return true;
    }

    if (current.value == "R@" && next.value == "!") {
        addOptimizedToken("R@_!");
        index += 1;
        optimizations++;
        return true;
    }

    if (current.type == TOKEN_VARIABLE) {
        if (next.value == "@") {
            addOptimizedToken("VAR_@", 0, current.value, current.word_id);
            index += 1;
            optimizations++;
            return true;
        }
        if (next.value == "!") {
            addOptimizedToken("VAR_!", 0, current.value, current.word_id);
            index += 1;
            optimizations++;
            return true;
        }
        if (next.value == ">R") {
            addOptimizedToken("VAR_TOR", 0, current.value, current.word_id);
            index += 1;
            optimizations++;
            return true;
        }
    }

    if (current.value == "C@" && next.value == "EMIT") {
        addOptimizedToken("C@_EMIT");
        index += 1;
        optimizations++;
        return true;
    }

    if (current.value == "DUP" && next.value == "+") {
        addOptimizedToken("LEA_TOS");
        index += 1;
        optimizations++;
        return true;
    }

    if (current.value == "SWAP" && next.value == "DROP") {
        addOptimizedToken("MOV_TOS_1");
        index += 1;
        optimizations++;
        return true;
    }

    if (current.value == "DUP" && next.value == "ROT") {
        addOptimizedToken("TUCK");
        index += 1;
        optimizations++;
        return true;
    }

    if (current.value == "OVER" && next.value == "DROP") {
        addOptimizedToken("DUP");
        index += 1;
        optimizations++;
        return true;
    }

    return false;
}

bool Optimizer::is_power_of_two(int64_t value) {
    return (value > 0 && (value & (value - 1)) == 0);
}

ForthToken Optimizer::create_optimized_token(const std::string &optimized_op) {
    ForthToken temp;
    temp.type = TOKEN_OPTIMIZED;
    temp.optimized_op = optimized_op;
    set_common_fields(temp);

    return temp;
}

void Optimizer::set_common_fields(ForthToken &token) {
    token.word_id = SymbolTable::instance().addSymbol(token.optimized_op);
    token.word_len = token.optimized_op.size();
    token.opt_value = token.int_value;

}
