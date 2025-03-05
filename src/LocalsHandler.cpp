#include "LocalsHandler.h"

void LocalsHandler::enterScope() {
    scopeStack.emplace(); // Push a new empty scope onto the stack
}

void LocalsHandler::exitScope() {
    if (scopeStack.empty()) {
        throw std::runtime_error("LocalsHandler: No active scope to exit.");
    }
    scopeStack.pop(); // Remove the current scope
}

void LocalsHandler::setLocal(const std::string& name, int64_t value) {
    if (scopeStack.empty()) {
        throw std::runtime_error("LocalsHandler: Cannot set local variable outside of a scope.");
    }
    scopeStack.top()[name] = value; // Store variable in the topmost scope
}

int64_t LocalsHandler::getLocal(const std::string& name) const {
    if (scopeStack.empty()) {
        throw std::runtime_error("LocalsHandler: No active scope to retrieve a local variable.");
    }

    auto& currentScope = scopeStack.top();
    auto it = currentScope.find(name);
    if (it == currentScope.end()) {
        throw std::runtime_error("LocalsHandler: Local variable '" + name + "' not found in current scope.");
    }
    return it->second;
}

bool LocalsHandler::hasLocal(const std::string& name) const {
    if (scopeStack.empty()) {
        return false;
    }
    return scopeStack.top().find(name) != scopeStack.top().end();
}
