#ifndef VARIABLESTORAGE_H
#define VARIABLESTORAGE_H

#include "Singleton.h"
#include <unordered_map>
#include <string>
#include <stdexcept>

class VariableStorage : public Singleton<VariableStorage> {
    friend class Singleton<VariableStorage>;

public:
    // Stores a global variable (persists for entire execution)
    void setGlobal(const std::string& name, int64_t value);

    // Retrieves a global variable
    int64_t getGlobal(const std::string& name) const;

    // Stores a transient variable (temporary, can be cleared)
    void setTransient(const std::string& name, int64_t value);

    // Retrieves a transient variable
    int64_t getTransient(const std::string& name) const;

    // Clears all transient variables
    void clearTransient();

    // Checks if a global variable exists
    bool hasGlobal(const std::string& name) const;

    // Checks if a transient variable exists
    bool hasTransient(const std::string& name) const;

private:
    VariableStorage() = default;

    // Global variables persist across executions
    std::unordered_map<std::string, int64_t> globalVariables;

    // Transient variables are temporary and can be cleared
    std::unordered_map<std::string, int64_t> transientVariables;
};

#endif // VARIABLESTORAGE_H
