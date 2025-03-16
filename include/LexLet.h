#ifndef LEXLET_H
#define LEXLET_H

#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <unordered_map>
#include <stdexcept>

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
    std::size_t position; // to improve error messages (index in input string)
};

// Utility: convert string to uppercase
inline std::string toUpper(const std::string &s) {
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

inline std::vector<let_token> tokenize(const std::string &input) {
    // std::cout << "input: " << input << std::endl;

    static const std::unordered_map<std::string, let_token_type> keywords = {
        {"LET", let_token_type::KEYWORD},
        {"FN", let_token_type::KEYWORD},
        {"WHERE", let_token_type::KEYWORD},
    };

    static const std::unordered_map<std::string, let_token_type> functions = {
        {"sqrt", let_token_type::FUNC},
        {"sin", let_token_type::FUNC},
        {"cos", let_token_type::FUNC},
        {"tan", let_token_type::FUNC},
        {"exp", let_token_type::FUNC},
        {"log", let_token_type::FUNC},
        {"ln", let_token_type::FUNC},
        {"fabs", let_token_type::FUNC},
        {"abs", let_token_type::FUNC},
        {"sinh", let_token_type::FUNC},
        {"cosh", let_token_type::FUNC},
        {"tanh", let_token_type::FUNC},
        {"asin", let_token_type::FUNC},
        {"acos", let_token_type::FUNC},
        {"atan", let_token_type::FUNC},
        {"log2", let_token_type::FUNC},
        {"log10", let_token_type::FUNC},
        {"atan2", let_token_type::FUNC},
        {"pow", let_token_type::FUNC},
        {"hypot", let_token_type::FUNC},
        {"fmod", let_token_type::FUNC},
        {"remainder", let_token_type::FUNC},
        {"fmin", let_token_type::FUNC},
        {"fmax", let_token_type::FUNC},
        {"display", let_token_type::FUNC}
    };

    std::vector<let_token> tokens;
    std::size_t i = 0;
    const std::size_t n = input.size();

    while (i < n) {
        // Skip whitespace
        if (std::isspace(static_cast<unsigned char>(input[i]))) {
            i++;
            continue;
        }

        // Parentheses or delimiter
        if (isPunctuationChar(input[i])) {
            let_token t;
            t.text = input.substr(i, 1);
            t.position = i;
            if (input[i] == '(' || input[i] == ')') {
                t.type = let_token_type::PAREN;
            } else {
                t.type = let_token_type::DELIM;
            }
            tokens.push_back(t);
            i++;
            continue;
        }

        // Operators (single character)
        if (isOperatorChar(input[i])) {
            let_token t;
            t.text = input.substr(i, 1);
            t.position = i;
            t.type = let_token_type::OP;
            tokens.push_back(t);
            i++;
            continue;
        }

        // Attempt numeric literal (possibly with exponent)
        if (std::isdigit(static_cast<unsigned char>(input[i])) || input[i] == '.') {
            std::size_t start = i;
            bool hasDecimal = false;
            bool hasExponent = false;

            // Parse integral/decimal part
            while (i < n) {
                char c = input[i];
                if (std::isdigit(static_cast<unsigned char>(c))) {
                    i++;
                } else if (c == '.' && !hasDecimal && !hasExponent) {
                    hasDecimal = true;
                    i++;
                } else if ((c == 'e' || c == 'E') && !hasExponent) {
                    // parse exponent
                    hasExponent = true;
                    i++;
                    // optional sign after exponent
                    if (i < n && (input[i] == '+' || input[i] == '-')) {
                        i++;
                    }
                } else {
                    break;
                }
            }

            std::string number = input.substr(start, i - start);
            // Basic validation could be done here. We assume it's valid for brevity.
            let_token t{number, let_token_type::NUM, start};
            tokens.push_back(t);
            continue;
        }

        // Attempt identifiers (keywords, functions, or variables)
        if (std::isalpha(static_cast<unsigned char>(input[i]))) {
            std::size_t start = i;
            // Gather alphanumeric + underscores
            while (i < n && (std::isalnum(static_cast<unsigned char>(input[i])) || input[i] == '_')) {
                i++;
            }

            std::string word = input.substr(start, i - start);
            std::string uppercaseWord = toUpper(word);

            let_token t;
            t.text = word;
            t.position = start;

            auto kwIt = keywords.find(uppercaseWord);
            auto fnIt = functions.find(word);
            if (kwIt != keywords.end()) {
                t.type = let_token_type::KEYWORD;
            } else if (fnIt != functions.end()) {
                t.type = let_token_type::FUNC;
            } else {
                // Treat all other valid identifiers as variables
                t.type = let_token_type::VAR;
            }
            tokens.push_back(t);
            continue;
        }

        // If none match, it's UNKNOWN
        let_token t;
        t.text = input.substr(i, 1);
        t.position = i;
        t.type = let_token_type::UNKNOWN;
        tokens.push_back(t);
        i++;
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
        std::cout << ") at pos=" << tk.position << "\n";
    }

    return 0;
}

#endif // LEXLET_H
