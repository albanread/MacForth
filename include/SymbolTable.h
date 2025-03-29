#ifndef SYMBOL_TABLE_H
#define SYMBOL_TABLE_H

#include <unordered_map>
#include <string>
#include <iostream>
#include <cstdint>

class SymbolTable {
public:
    static SymbolTable& instance() {
        static SymbolTable instance;
        return instance;
    }

    // Adds a new word or returns the existing ID if already present
    uint32_t addSymbol(const std::string& name) {
        auto it = symbols.find(name);
        if (it != symbols.end()) {
            return it->second;
        }
        uint32_t id = next_id++;
        symbols[name] = id;
        reverse_lookup[id] = name;
        return id;
    }

    uint32_t findSymbol(const std::string& name) {
        auto it = symbols.find(name);
        if (it != symbols.end()) {
            return it->second;
        }
        return 0;
    }

    // Removes a symbol if it exists
    bool forgetSymbol(const std::string& name) {
        auto it = symbols.find(name);
        if (it != symbols.end()) {
            uint32_t id = it->second;

            // Remove from both maps
            symbols.erase(it);
            reverse_lookup.erase(id);
            return true;
        }
        return false; // Symbol wasn't found
    }


    uint32_t definedSymbol(const std::string& name) {
        auto it = symbols.find(name);
        if (it != symbols.end()) {
            return it->second;
        }
        return 0; // not defined
    }


    // Gets the string name from an ID
    [[nodiscard]] std::string getSymbol(const uint32_t id) const {
        auto it = reverse_lookup.find(id);
        if (it != reverse_lookup.end()) {
            return it->second;
        }
        return "";
    }

    void printSymbols() const {
        for (auto& [name, id] : symbols) {
            std::cout << name << " " << id << std::endl;
        }
    }

private:
    SymbolTable() = default;
    std::unordered_map<std::string, uint32_t> symbols;
    std::unordered_map<uint32_t, std::string> reverse_lookup;
    uint32_t next_id = 1; // Start from 1 (0 can be used as NULL_ID)
};


#endif // SYMBOL_TABLE_H
