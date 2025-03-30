#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "Singleton.h"
#include "Tokenizer.h"
#include <deque>

class Optimizer : public Singleton<Optimizer> {
    friend class Singleton<Optimizer>; // Allow access to private constructor for Singleton

public:
    int optimize(const std::deque<ForthToken> &tokens, std::deque<ForthToken> &optimized_tokens);

    bool is_arithmetic_operator(const std::string &op);

    bool is_comparison_operator(const std::string &op);

    bool optimize_constant_operation(const std::deque<ForthToken> &tokens, std::deque<ForthToken> &optimized_tokens,
                                     size_t index);

    bool optimize_literal_comparison(const std::deque<ForthToken> &tokens, std::deque<ForthToken> &optimized_tokens,
                                     size_t index);

    ForthToken getToken(const std::deque<ForthToken> &tokens, size_t i);

    bool optimize_peephole_case(const std::deque<ForthToken> &tokens, std::deque<ForthToken> &optimized_tokens,
                                size_t &index);

    bool is_power_of_two(int64_t value);

    ForthToken create_optimized_token(const std::string &optimized_op);

    void set_common_fields(ForthToken &token);

private:
    // Private constructor and destructor
    Optimizer() = default;
    ~Optimizer() = default;



    // Helper methods for folding and optimizing
    bool fold_constants(std::deque<ForthToken> &tokens, size_t start, size_t end);
    void optimize_literal_comparisons(std::deque<ForthToken> &tokens, std::deque<ForthToken> &optimized_tokens);
};

#endif // OPTIMIZER_H