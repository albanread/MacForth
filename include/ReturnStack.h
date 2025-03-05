#ifndef RETURNSTACK_H
#define RETURNSTACK_H

#include "Singleton.h"
#include <stack>
#include <stdexcept>

class ReturnStack : public Singleton<ReturnStack> {
    friend class Singleton<ReturnStack>;

public:
    // Pushes a return address onto the stack
    void push(int64_t value);

    // Pops a return address from the stack
    int64_t pop();

    // Gets the top value without popping
    int64_t top() const;

    // Checks if the return stack is empty
    bool isEmpty() const;

    // Clears the stack
    void clear();

private:
    ReturnStack() = default;

    std::stack<int64_t> stack;
};

#endif // RETURNSTACK_H
