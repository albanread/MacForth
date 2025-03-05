#include "DataStack.h"

void DataStack::push(int value) {
    stack.push(value);
}

int DataStack::pop() {
    if (stack.empty()) return 0;
    int value = stack.top();
    stack.pop();
    return value;
}

int DataStack::top() const {
    return stack.empty() ? 0 : stack.top();
}
