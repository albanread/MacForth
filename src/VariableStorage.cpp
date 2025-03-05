#include "VariableStorage.h"

void VariableStorage::setGlobal(const std::string& name, int64_t value) {
    globalVariables[name] = value;
}

int64_t VariableStorage::getGlobal(const std::string& name) const {
    auto it = globalVariables.find(name);
    if (it != globalVariables.end()) {
        return it->second;
    }
    throw std::runtime_error("VariableStorage: Global variable '" + name + "' not found.");
}

void VariableStorage::setTransient(const std::string& name, int64_t value) {
    transientVariables[name] = value;
}

int64_t VariableStorage::getTransient(const std::string& name) const {
    auto it = transientVariables.find(name);
    if (it != transientVariables.end()) {
        return it->second;
    }
    throw std::runtime_error("VariableStorage: Transient variable '" + name + "' not found.");
}

void VariableStorage::clearTransient() {
    transientVariables.clear();
}

bool VariableStorage::hasGlobal(const std::string& name) const {
    return globalVariables.find(name) != globalVariables.end();
}

bool VariableStorage::hasTransient(const std::string& name) const {
    return transientVariables.find(name) != transientVariables.end();
}
