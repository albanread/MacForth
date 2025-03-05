#include "ReturnStack.h"

void ReturnStack::push(int64_t value) {
    stack.push(value);
}

int64_t ReturnStack::pop() {
    if (stack.empty()) {
        throw std::underflow_error("ReturnStack underflow: Attempted to pop from an empty return stack.");
    }
    int64_t value = stack.top();
    stack.pop();
    return value;
}

int64_t ReturnStack::top() const {
    if (stack.empty()) {
        throw std::underflow_error("ReturnStack error: Attempted to access top of an empty return stack.");
    }
    return stack.top();
}

bool ReturnStack::isEmpty() const {
    return stack.empty();
}

void ReturnStack::clear() {
    while (!stack.empty()) {
        stack.pop();
    }
}
