#ifndef LOCALSHANDLER_H
#define LOCALSHANDLER_H

#include "Singleton.h"
#include <unordered_map>
#include <stack>
#include <string>
#include <cstdint>
#include <stdexcept>

class LocalsHandler : public Singleton<LocalsHandler> {
    friend class Singleton<LocalsHandler>;

public:
    // Enters a new scope (creates a new local context)
    void enterScope();

    // Exits the current scope (removes the topmost local context)
    void exitScope();

    // Defines a local variable in the current scope
    void setLocal(const std::string& name, int64_t value);

    // Retrieves a local variable from the current scope
    int64_t getLocal(const std::string& name) const;

    // Checks if a local variable exists in the current scope
    bool hasLocal(const std::string& name) const;

private:
    LocalsHandler() = default;

    // Stack of local variable maps (each function call has its own scope)
    std::stack<std::unordered_map<std::string, int64_t>> scopeStack;
};

#endif // LOCALSHANDLER_H
