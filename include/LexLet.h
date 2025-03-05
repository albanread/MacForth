#ifndef LEXLET_H
#define LEXLET_H

#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <unordered_map>

// let_token types
enum class let_token_type {
    KEYWORD,
    FUNC,
    OP,
    VAR,
    NUM,
    PAREN,
    DELIM,
    UNKNOWN
};

struct let_token {
    std::string text;
    let_token_type type;
};

// Utility: convert string to uppercase
std::string toUpper(const std::string &s) {
    std::string result = s;
    for (auto &c: result) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return result;
}

// Utility functions for character classification
constexpr bool isOperatorChar(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '=';
}

constexpr bool isDelimiterChar(char c) {
    return c == ',' || c == ';';
}

constexpr bool isPunctuationChar(char c) {
    return c == '(' || c == ')' || isDelimiterChar(c);
}


bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool isValidNumber(const std::string &text) {
    bool hasDot = false;
    for (char c: text) {
        if (c == '.') {
            if (hasDot) {
                return false; // More than one '.' is invalid
            }
            hasDot = true;
        } else if (!isDigit(c)) {
            return false; // Non-digit character found
        }
    }
    return true; // Valid numeric literal
}


// The main tokenizer function
std::vector<let_token> tokenize(const std::string &input) {
    // Maps for quick identification of keywords and functions
    static const std::unordered_map<std::string, let_token_type> keywords = {
        {"LET", let_token_type::KEYWORD},
        {"FN", let_token_type::KEYWORD},
        {"WHERE", let_token_type::KEYWORD}
    };

    static const std::unordered_map<std::string, let_token_type> functions = {
        {"sqrt", let_token_type::FUNC},
        {"sin", let_token_type::FUNC},
        {"cos", let_token_type::FUNC},
        {"exp", let_token_type::FUNC},
        {"ln", let_token_type::FUNC}
    };

    std::vector<let_token> tokens;
    std::size_t i = 0;

    while (i < input.size()) {
        // Skip whitespace
        if (std::isspace(static_cast<unsigned char>(input[i]))) {
            i++;
            continue;
        }

        // Parentheses and delimiters
        if (isPunctuationChar(input[i])) {
            let_token t;
            t.text = input.substr(i, 1);
            t.type = (input[i] == '(' || input[i] == ')') ? let_token_type::PAREN : let_token_type::DELIM;
            tokens.push_back(t);
            i++;
            continue;
        }

        // Operators (single character only)
        if (isOperatorChar(input[i])) {
            let_token t;
            t.text = input.substr(i, 1);
            t.type = let_token_type::OP;
            tokens.push_back(t);
            i++;
            continue;
        }

        // Attempt to parse numbers (including decimal part)
        if (std::isdigit(static_cast<unsigned char>(input[i])) || input[i] == '.') {
            std::size_t start = i;
            bool hasDecimal = (input[i] == '.'); // Track if decimal point is encountered

            i++;

            while (i < input.size()) {
                if (std::isdigit(static_cast<unsigned char>(input[i]))) {
                    i++;
                } else if (input[i] == '.' && !hasDecimal) {
                    hasDecimal = true; // First decimal point is valid
                    i++;
                } else {
                    break; // Stop when encountering invalid characters
                }
            }

            // Extract the number from the input
            std::string number = input.substr(start, i - start);

            // Check if the number is valid: it cannot contain multiple '.' or non-digits
            if (!hasDecimal || (hasDecimal && number.size() > 1)) {
                // Add the number as a NUM token
                tokens.push_back({number, let_token_type::NUM });
            } else {
                throw std::runtime_error("Invalid number format: " + number);
            }

            continue;
        }


        // Attempt to parse identifiers (keywords, functions, or variables)
        if (std::isalpha(static_cast<unsigned char>(input[i]))) {
            std::size_t start = i;
            while (i < input.size() &&
                   (std::isalnum(static_cast<unsigned char>(input[i])) || input[i] == '_')) {
                i++;
            }

            std::string word = input.substr(start, i - start);
            std::string uppercaseWord = toUpper(word);

            auto kwIt = keywords.find(uppercaseWord);
            auto fnIt = functions.find(word);

            if (kwIt != keywords.end()) {
                tokens.push_back({word, let_token_type::KEYWORD});
            } else if (fnIt != functions.end()) {
                tokens.push_back({word, let_token_type::FUNC});
            } else if (word.size() == 1 && std::islower(static_cast<unsigned char>(word[0]))) {
                // Single-letter variable (a-z only)
                tokens.push_back({word, let_token_type::VAR});
            } else {
                // Unknown identifier
                tokens.push_back({word, let_token_type::UNKNOWN});
            }
            continue;
        }

        // If none of the above apply, mark as UNKNOWN
        {
            let_token t;
            t.text = input.substr(i, 1);
            t.type = let_token_type::UNKNOWN;
            tokens.push_back(t);
            i++;
        }
    }

    return tokens;
}

// Example usage:
inline int test_lex() {
    std::string input = "LET (x, y) = FN(a, b) = a + b * sqrt(a) WHERE b = 2.0;";
    auto tokens = tokenize(input);

    // Print out the tokens for demonstration
    for (const auto &tk: tokens) {
        std::cout << "(" << tk.text << ", ";
        switch (tk.type) {
            case let_token_type::KEYWORD: std::cout << "KEYWORD";
                break;
            case let_token_type::FUNC: std::cout << "FUNC";
                break;
            case let_token_type::OP: std::cout << "OP";
                break;
            case let_token_type::VAR: std::cout << "VAR";
                break;
            case let_token_type::NUM: std::cout << "NUM";
                break;
            case let_token_type::PAREN: std::cout << "PAREN";
                break;
            case let_token_type::DELIM: std::cout << "DELIM";
                break;
            default: std::cout << "UNKNOWN";
                break;
        }
        std::cout << ")\n";
    }

    return 0;
}

#endif // LEXLET_H
