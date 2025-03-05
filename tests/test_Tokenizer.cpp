#include "Tokenizer.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include "CodeGenerator.h"
#include "Optimizer.h"

// Helper to create a tokenizer and tokenize input
std::deque<ForthToken> tokenizeInput(const std::string &input) {
    std::deque<ForthToken> tokens;
    Tokenizer::instance().tokenize_forth(input, tokens);
    return tokens;
}

// Test 1: Simple word tokenization
TEST(TokenizerTests, TokenizeWords) {
    code_generator_initialize();
    std::string input = "DUP ROT SWAP";
    auto tokens = tokenizeInput(input);
    Tokenizer::instance().print_token_list(tokens);
    std::cout << "Tokens size: " << tokens.size() << std::endl;
    ASSERT_EQ(tokens.size(), 4); // 3 words + 1 TOKEN_END
    EXPECT_EQ(tokens[0].type, TOKEN_WORD);
    EXPECT_EQ(tokens[0].value, "DUP");
    EXPECT_EQ(tokens[1].type, TOKEN_WORD);
    EXPECT_EQ(tokens[1].value, "ROT");
    EXPECT_EQ(tokens[2].type, TOKEN_WORD);
    EXPECT_EQ(tokens[2].value, "SWAP");
    EXPECT_EQ(tokens[3].type, TOKEN_END);
}

// Test 2: Number tokenization
TEST(TokenizerTests, TokenizeNumbers) {
    code_generator_initialize();
    std::string input = "123 0x1A -456";
    auto tokens = tokenizeInput(input);
    std::cout << "Tokens size: " << tokens.size() << std::endl;
    ASSERT_EQ(tokens.size(), 4); // 3 numbers + 1 TOKEN_END
    EXPECT_EQ(tokens[0].type, TOKEN_NUMBER);
    EXPECT_EQ(tokens[0].int_value, 123);
    EXPECT_EQ(tokens[1].type, TOKEN_NUMBER);
    EXPECT_EQ(tokens[1].int_value, 26); // 0x1A in decimal
    EXPECT_EQ(tokens[2].type, TOKEN_NUMBER);
    EXPECT_EQ(tokens[2].int_value, -456);
    EXPECT_EQ(tokens[3].type, TOKEN_END);
}

// Test 3: Floating-point numbers
TEST(TokenizerTests, TokenizeFloats) {
    code_generator_initialize();
    std::string input = "3.14 -2.71 1.0e3";
    auto tokens = tokenizeInput(input);
    std::cout << "Tokens size: " << tokens.size() << std::endl;
    ASSERT_EQ(tokens.size(), 4); // 3 floats + 1 TOKEN_END
    EXPECT_EQ(tokens[0].type, TOKEN_FLOAT);
    EXPECT_FLOAT_EQ(tokens[0].float_value, 3.14);
    EXPECT_EQ(tokens[1].type, TOKEN_FLOAT);
    EXPECT_FLOAT_EQ(tokens[1].float_value, -2.71);
    EXPECT_EQ(tokens[2].type, TOKEN_FLOAT);
    EXPECT_FLOAT_EQ(tokens[2].float_value, 1000.0); // 1.0e3
    EXPECT_EQ(tokens[3].type, TOKEN_END);
}

// Test 4: Code constructs
TEST(TokenizerTests, TokenizeProgrammingConstructs) {
    code_generator_initialize();
    std::string input = ": ; ( ) { }";
    auto tokens = tokenizeInput(input);
    std::cout << "Tokens size: " << tokens.size() << std::endl;
    ASSERT_EQ(tokens.size(), 7); // 6 constructs + 1 TOKEN_END
    EXPECT_EQ(tokens[0].type, TOKEN_COMPILING);
    EXPECT_EQ(tokens[1].type, TOKEN_INTERPRETING);
    EXPECT_EQ(tokens[2].type, TOKEN_BEGINCOMMENT);
    EXPECT_EQ(tokens[3].type, TOKEN_ENDCOMMENT);
    EXPECT_EQ(tokens[4].type, TOKEN_BEGINLOCALS);
    EXPECT_EQ(tokens[5].type, TOKEN_ENDLOCALS);
    EXPECT_EQ(tokens[6].type, TOKEN_END);
}

// Test 5: String literals
TEST(TokenizerTests, TokenizeStringLiterals) {
    code_generator_initialize();
    std::string input = "DUP .\" this is a string\" ROT";
    auto tokens = tokenizeInput(input);
    std::cout << "Tokens size: " << tokens.size() << std::endl;
    Tokenizer::instance().print_token_list(tokens);

    ASSERT_EQ(tokens.size(), 5); // 3 words + 1 string + 1 TOKEN_END
    EXPECT_EQ(tokens[0].type, TOKEN_WORD);
    EXPECT_EQ(tokens[0].value, "DUP");
    EXPECT_EQ(tokens[1].type, TOKEN_WORD);
    EXPECT_EQ(tokens[1].value, ".\"");
    EXPECT_EQ(tokens[2].type, TOKEN_STRING);
    EXPECT_EQ(tokens[2].value, "this is a string");
    EXPECT_EQ(tokens[3].type, TOKEN_WORD);
    EXPECT_EQ(tokens[3].value, "ROT");
    EXPECT_EQ(tokens[4].type, TOKEN_END);
}

// Test 7: Handle empty input
TEST(TokenizerTests, HandleEmptyInput) {
    code_generator_initialize();
    std::string input = "";
    auto tokens = tokenizeInput(input);
    std::cout << "Tokens size: " << tokens.size() << std::endl;
    ASSERT_EQ(tokens.size(), 1); // Only TOKEN_END
    EXPECT_EQ(tokens[0].type, TOKEN_END);
}

// Test 8: Whitespace handling
TEST(TokenizerTests, HandleWhitespaces) {
    code_generator_initialize();
    std::string input = "   DUP   123   ";
    auto tokens = tokenizeInput(input);
    std::cout << "Tokens size: " << tokens.size() << std::endl;
    ASSERT_EQ(tokens.size(), 3); // 2 tokens (word/number) + 1 TOKEN_END
    EXPECT_EQ(tokens[0].type, TOKEN_WORD);
    EXPECT_EQ(tokens[0].value, "DUP");
    EXPECT_EQ(tokens[1].type, TOKEN_NUMBER);
    EXPECT_EQ(tokens[1].int_value, 123);
    EXPECT_EQ(tokens[2].type, TOKEN_END);
}


// Test constant folding optimization
TEST(OptimizerTest, ConstantFolding_Addition) {
    code_generator_initialize();
    std::deque<ForthToken> tokens = {
        ForthToken{TOKEN_NUMBER, 10},
        ForthToken{TOKEN_WORD, "+", 0}
    };
    std::deque<ForthToken> optimized_tokens;

    Optimizer::instance().optimize(tokens, optimized_tokens);
    std::cout << "Tokens size: " << tokens.size() << std::endl;
    std::cout << "Optimized Tokens size: " << optimized_tokens.size() << std::endl;
    ASSERT_EQ(optimized_tokens.size(), 2);
    EXPECT_EQ(optimized_tokens[0].type, TOKEN_OPTIMIZED);
    EXPECT_EQ(optimized_tokens[0].optimized_op, "ADD_IMM");
    EXPECT_EQ(optimized_tokens[0].int_value, 10);
}

// Test strength reduction for multiplication by power of 2
TEST(OptimizerTest, StrengthReduction_Multiplication) {
    code_generator_initialize();
    std::deque<ForthToken> tokens = {
        ForthToken(TOKEN_NUMBER, 4),
        ForthToken(TOKEN_WORD, "*", 0)
    };
    std::cout << "Tokens size: " << tokens.size() << std::endl;
    std::deque<ForthToken> optimized_tokens;
    Tokenizer::instance().print_token_list(tokens);
    Optimizer::instance().optimize(tokens, optimized_tokens);
    Tokenizer::instance().print_token_list(optimized_tokens);
    std::cout << "Optimized Tokens size: " << optimized_tokens.size() << std::endl;
    ASSERT_EQ(optimized_tokens.size(), 2);
    EXPECT_EQ(optimized_tokens[0].type, TOKEN_OPTIMIZED);
    EXPECT_EQ(optimized_tokens[0].optimized_op, "SHL_IMM");
    EXPECT_EQ(optimized_tokens[0].opt_value, 2);
}

TEST(OptimizerTest, StrengthReduction_Division) {
    code_generator_initialize();
    std::deque<ForthToken> tokens = {
        ForthToken(TOKEN_NUMBER, 8),
        ForthToken(TOKEN_WORD, "/", 0)
    };

    std::deque<ForthToken> optimized_tokens;

    Tokenizer::instance().print_token_list(tokens);
    Optimizer::instance().optimize(tokens, optimized_tokens);
    Tokenizer::instance().print_token_list(optimized_tokens);
    std::cout << "Optimized Tokens size: " << optimized_tokens.size() << std::endl;
    ASSERT_EQ(optimized_tokens.size(), 2);
    EXPECT_EQ(optimized_tokens[0].type, TOKEN_OPTIMIZED);
    EXPECT_EQ(optimized_tokens[0].optimized_op, "SHR_IMM");
    EXPECT_EQ(optimized_tokens[0].opt_value, 3); // 8 is 2^3
}

TEST(OptimizerTest, Multiplication_ByNonPowerOf2) {
    code_generator_initialize();
    std::deque<ForthToken> tokens = {
        ForthToken(TOKEN_NUMBER, 6),
        ForthToken(TOKEN_WORD, "*", 0)
    };

    std::deque<ForthToken> optimized_tokens;

    Tokenizer::instance().print_token_list(tokens);
    Optimizer::instance().optimize(tokens, optimized_tokens);
    Tokenizer::instance().print_token_list(optimized_tokens);
    std::cout << "Optimized Tokens size: " << optimized_tokens.size() << std::endl;
    ASSERT_EQ(optimized_tokens.size(), 2);
    EXPECT_EQ(optimized_tokens[0].type, TOKEN_OPTIMIZED);
    EXPECT_EQ(optimized_tokens[0].optimized_op, "MUL_IMM");
    EXPECT_EQ(optimized_tokens[0].opt_value, 6);
}

TEST(OptimizerTest, DivisionByZero) {
    code_generator_initialize();
    std::deque<ForthToken> tokens = {
        ForthToken(TOKEN_NUMBER, 0),
        ForthToken(TOKEN_WORD, "/", 0)
    };

    std::deque<ForthToken> optimized_tokens;

    EXPECT_THROW(Optimizer::instance().optimize(tokens, optimized_tokens);, std::runtime_error);
}

TEST(OptimizerTest, Addition_WithZero) {
    code_generator_initialize();
    std::deque<ForthToken> tokens = {
        ForthToken(TOKEN_NUMBER, 0),
        ForthToken(TOKEN_WORD, "+", 0)
    };

    std::deque<ForthToken> optimized_tokens;

    Tokenizer::instance().print_token_list(tokens);
    Optimizer::instance().optimize(tokens, optimized_tokens);
    Tokenizer::instance().print_token_list(optimized_tokens);
    std::cout << "Optimized Tokens size: " << optimized_tokens.size() << std::endl;
    ASSERT_EQ(optimized_tokens.size(), 2);
    EXPECT_EQ(optimized_tokens[0].type, TOKEN_OPTIMIZED);
    EXPECT_EQ(optimized_tokens[0].optimized_op, "ADD_IMM");
    EXPECT_EQ(optimized_tokens[0].opt_value, 0);
}


TEST(OptimizerTest, PeepholeOptimization_DupPlus) {
    std::deque<ForthToken> tokens = {
        ForthToken(TOKEN_WORD, "DUP", 0),
        ForthToken(TOKEN_WORD, "+", 0)
    };

    std::deque<ForthToken> optimized_tokens;

    Tokenizer::instance().print_token_list(tokens);
    Optimizer::instance().optimize(tokens, optimized_tokens);
    Tokenizer::instance().print_token_list(optimized_tokens);
    std::cout << "Optimized Tokens size: " << optimized_tokens.size() << std::endl;
    ASSERT_EQ(optimized_tokens.size(), 2);
    EXPECT_EQ(optimized_tokens[0].type, TOKEN_OPTIMIZED);
    EXPECT_EQ(optimized_tokens[0].optimized_op, "LEA_TOS");
    EXPECT_EQ(optimized_tokens[0].opt_value, 0);
}


TEST(OptimizerTest, EmptyInput) {
    std::deque<ForthToken> tokens = {};
    std::deque<ForthToken> optimized_tokens;

    Tokenizer::instance().print_token_list(tokens);
    Optimizer::instance().optimize(tokens, optimized_tokens);
    Tokenizer::instance().print_token_list(optimized_tokens);
    std::cout << "Optimized Tokens size: " << optimized_tokens.size() << std::endl;
    ASSERT_FALSE(optimized_tokens.empty());
    EXPECT_EQ(optimized_tokens[0].type, TOKEN_END);
}

TEST(OptimizerTest, Division_ByNonPowerOf2) {
    std::deque<ForthToken> tokens = {
        ForthToken(TOKEN_NUMBER, 7),
        ForthToken(TOKEN_WORD, "/", 0)
    };

    std::deque<ForthToken> optimized_tokens;

    Tokenizer::instance().instance().print_token_list(tokens);
    Optimizer::instance().optimize(tokens, optimized_tokens);
    Tokenizer::instance().instance().print_token_list(optimized_tokens);
    std::cout << "Optimized Tokens size: " << optimized_tokens.size() << std::endl;
    ASSERT_EQ(optimized_tokens.size(), 2);
    EXPECT_EQ(optimized_tokens[0].type, TOKEN_OPTIMIZED);
    EXPECT_EQ(optimized_tokens[0].optimized_op, "DIV_IMM");
    EXPECT_EQ(optimized_tokens[0].opt_value, 7);
}


TEST(OptimizerTest, NoPeepholeMatched) {
    std::deque<ForthToken> tokens = {
        ForthToken(TOKEN_WORD, "ROT", 0),
        ForthToken(TOKEN_WORD, "NIP", 0)
    };

    std::deque<ForthToken> optimized_tokens;

    Tokenizer::instance().print_token_list(tokens);
    Optimizer::instance().optimize(tokens, optimized_tokens);
    Tokenizer::instance().print_token_list(optimized_tokens);
    std::cout << "Optimized Tokens size: " << optimized_tokens.size() << std::endl;
    ASSERT_EQ(optimized_tokens.size(), tokens.size()+1);
    EXPECT_EQ(optimized_tokens[0].value, "ROT");
    EXPECT_EQ(optimized_tokens[1].value, "NIP");
}


// Test redundant operations like SWAP DROP
TEST(OptimizerTest, PeepholeOptimization_SwapDrop) {
    std::deque<ForthToken> tokens = {
        ForthToken(TOKEN_WORD, "SWAP", 0),
        ForthToken(TOKEN_WORD, "DROP", 0)
    };
    std::deque<ForthToken> optimized_tokens;

    Tokenizer::instance().print_token_list(tokens);
    Optimizer::instance().optimize(tokens, optimized_tokens);
    Tokenizer::instance().print_token_list(optimized_tokens);
    std::cout << "Optimized Tokens size: " << optimized_tokens.size() << std::endl;
    ASSERT_EQ(optimized_tokens.size(), 2);
    EXPECT_EQ(optimized_tokens[0].type, TOKEN_OPTIMIZED);
    EXPECT_EQ(optimized_tokens[0].optimized_op, "MOV_TOS_1");
}

// Test that unrelated tokens are left unchanged
TEST(OptimizerTest, NoOptimizationNeeded) {
    std::deque<ForthToken> tokens = {
        {TOKEN_WORD, "DUP", 0},
        {TOKEN_WORD, "!", 0}
    };
    std::deque<ForthToken> optimized_tokens;

    Tokenizer::instance().print_token_list(tokens);
    Optimizer::instance().optimize(tokens, optimized_tokens);
    Tokenizer::instance().print_token_list(optimized_tokens);

    std::cout << "Optimized Tokens size: " << optimized_tokens.size() << std::endl;
    ASSERT_EQ(optimized_tokens.size(), 3);
    EXPECT_EQ(optimized_tokens[0].type, TOKEN_WORD);
    EXPECT_EQ(optimized_tokens[1].type, TOKEN_WORD);
}
