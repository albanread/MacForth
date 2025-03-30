#include "Tokenizer.h"
#include <sstream>
#include <iostream>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <cstdint>
#include <cassert>
#include <deque>
#include <ForthDictionary.h>
#include "SymbolTable.h"

bool is_number(const std::string &s) {
    try {
        std::stoll(s, nullptr, 0);
        return true;
    } catch (...) {
        return false;
    }
}

int64_t parseNumber(const std::string &word) {
    try {
        return std::stoll(word, nullptr, 0);
    } catch (...) {
        throw std::invalid_argument("Invalid number: " + word);
    }
}


void Tokenizer::print_token(const ForthToken &token) {
    switch (token.type) {
        case TOKEN_END:
            std::cout << "END:" << '\t' << "\n";
            break;
        case TOKEN_WORD:
            std::cout << "WORD:" << '\t' << '[' << token.value << "]\n";
            break;
        case TOKEN_NUMBER:
            std::cout << "NUMBER:" << '\t' << '[' << token.int_value << "]\n";
            break;
        case TOKEN_FLOAT:
            std::cout << "FLOAT: " << '\t' << '[' << token.float_value << "]\n";
            break;
        case TOKEN_STRING:
            std::cout << "STRING:" << '\t' << '\"' << token.value << "\"\n";
            break;
        case TOKEN_COMPILING:
            std::cout << "COMPILING:\n";
            break;
        case TOKEN_INTERPRETING:
            std::cout << "INTERPRETING:\n";
            break;
        case TOKEN_BEGINCOMMENT:
            std::cout << "In COMMENT\n";
            break;
        case TOKEN_OPTIMIZED:
            std::cout << "OPTIMIZED:";
            std::cout << token.optimized_op;
            std::cout << " with constant:" << '[' << token.int_value << "] "
                    << "id: [" << token.word_id << "] "
                    << "len: [" << token.word_len << "] " << std::endl;

            break;

        default:
            std::cout << "UNKNOWN\n";
            break;
    }
}

void Tokenizer::print_token_list(const std::deque<ForthToken> &tokens) {
    int count = 0;
    for (const auto &token: tokens) {
        std::cout << "[" << count++ << "] - ";
        print_token(token);
        if (token.type == TOKEN_END) break;
    }
    std::cout << std::endl;
}

void skip_whitespace(const char **input) {
    while (**input && isspace(**input)) {
        (*input)++;
    }
}


int is_float(const char *str) {
    int has_dot = 0, has_exp = 0;
    int seen_digit_before_exp = 0, seen_digit_after_exp = 0;

    // Handle optional leading sign
    if (*str == '-' || *str == '+') {
        str++;
    }

    while (*str) {
        if (*str == '.') {
            if (has_dot || has_exp) {
                return 0; // Invalid: more than one dot or dot after exponent
            }
            has_dot = 1;
        } else if (*str == 'e' || *str == 'E') {
            if (has_exp || !seen_digit_before_exp) {
                return 0; // Invalid: more than one exponent or no digits before exponent
            }
            has_exp = 1;
        } else if (*str == '-' || *str == '+') {
            if (*(str - 1) != 'e' && *(str - 1) != 'E') {
                return 0; // Invalid: sign not immediately after exponent
            }
        } else if (isdigit(*str)) {
            if (has_exp) {
                seen_digit_after_exp = 1; // Keeping track of digits after 'e/E'
            } else {
                seen_digit_before_exp = 1; // Keeping track of digits before 'e/E'
            }
        } else {
            return 0; // Invalid character
        }
        str++;
    }
    return (has_dot || has_exp) && seen_digit_before_exp && (!has_exp || seen_digit_after_exp);
}


ForthToken Tokenizer::get_next_token(const char **input) {
    ForthToken token; // Default initialization of all fields

    skip_whitespace(input); // Skip leading whitespace

    // Handle end of input
    if (**input == '\0') {
        token.type = TOKEN_END;
        return token;
    }

    int i = 0;
    char temp[MAX_TOKEN_LENGTH] = {0};

    // 🔥 Improved token parsing: Allow words to end in `"`, but not start with it
    while (**input && !isspace(**input) && i < MAX_TOKEN_LENGTH - 1) {
        char c = *(*input)++;

        // Allow `"` **only at the end of a word**
        if (c == '"' && i > 0) {
            temp[i++] = c;
            break; // Stop here if we found a `"` at the end
        }

        temp[i++] = c;
    }
    temp[i] = '\0';

    // 🔍 Check if the token is a word ending in `"`, like `S"` or `."`
    if (i > 1 && temp[i - 1] == '"') {
        token.type = TOKEN_WORD;
        token.value = temp; // Store the full word (e.g., `."`, `S"`)
        token.word_len = strlen(temp);
        token.word_id = SymbolTable::instance().addSymbol(temp);
        return token;
    }

    // ✅ Regular word lookup
    if (auto word_id = SymbolTable::instance().definedSymbol(temp); word_id != 0) {
        token.type = TOKEN_WORD;
        token.value = temp;
        token.word_id = word_id;
        token.word_len = strlen(temp);
        if (const auto &dict = ForthDictionary::instance(); dict.isVariable(temp)) {
            token.type = TOKEN_VARIABLE;
        }
        return token;
    }

    // ✅ Other token types (numbers, comments, special symbols)
    if (strcmp(temp, ":") == 0) {
        token.type = TOKEN_COMPILING;
    } else if (strcmp(temp, ";") == 0) {
        token.type = TOKEN_INTERPRETING;
    } else if (strcmp(temp, "(") == 0) {
        token.type = TOKEN_BEGINCOMMENT;
        inComment = true;
    } else if (strcmp(temp, ")") == 0) {
        token.type = TOKEN_ENDCOMMENT;
        inComment = false;
    } else if (strcmp(temp, "{") == 0) {
        token.type = TOKEN_BEGINLOCALS;
        token.value = temp;
    } else if (strcmp(temp, "}") == 0) {
        token.type = TOKEN_ENDLOCALS;
    } else if (is_float(temp)) {
        token.type = TOKEN_FLOAT;
        token.float_value = atof(temp);
    } else if (is_number(temp)) {
        token.type = TOKEN_NUMBER;
        token.int_value = parseNumber(temp);
    } else {
        // and unknown word may be swallowed by CREATE, VARIABLE, CONSTANT etc
        token.type = TOKEN_UNKNOWN;
        token.value = temp;

        //std::cerr << "Error: Unknown token: " << temp << std::endl;
        //raise_c(5);
    }

    return token;
}

ForthToken get_string_token(const char **input) {
    ForthToken token;
    token.type = TOKEN_STRING;
    token.value = ""; // Initialize value as empty string

    if (**input == '\0') return token; // 🚨 Prevent buffer overrun

    (*input)++; // Consume opening quote

    // Handle optional space after opening quote
    if (**input == ' ') {
        (*input)++; // Consume the optional space
    }

    // 🚨 Prevent infinite loops and buffer overruns
    int max_length = MAX_INPUT; // Avoid unbounded loops
    while (**input && **input != '"' && max_length-- > 0) {
        token.value += *(*input)++;
    }

    // 🚨 Avoid dereferencing null pointers
    if (**input == '"') {
        (*input)++; // Consume closing quote safely
    } else {
        std::cerr << "Error: Missing closing quote for string!" << std::endl;
    }

    return token;
}


int Tokenizer::tokenize_forth(const std::string &input, std::deque<ForthToken> &tokens) {
    const char *cursor = input.c_str(); // Pointer for traversing input
    tokens.clear(); // Clear any pre-existing tokens

    while (true) {
        ForthToken token = instance().get_next_token(&cursor); // Get the next token

        // Stop at end of input
        if (token.type == TOKEN_END) {
            tokens.push_back(token);
            break;
        }

        // 🔥 General case: Any word ending in `"` should collect a string
        if (token.type == TOKEN_WORD && !token.value.empty() && token.value.back() == '"') {
            tokens.push_back(token); // Push the Forth word (e.g., `S"`, `."`)
            ForthToken string_token = get_string_token(&cursor); // Get the string
            tokens.push_back(string_token); // Push the string as a separate token
        } else {
            tokens.push_back(token); // Push regular tokens
        }
    }

    //
    return tokens.size(); // Return the total number of tokens
}
