#ifndef FORTH_DICTIONARY_ENTRY_H
#define FORTH_DICTIONARY_ENTRY_H

#include <cstdint>
#include <deque>
#include <iostream>
#include <string> // Include this to avoid potential std::string issues
#include "WordHeap.h"
#include "SymbolTable.h"
#include "Tokenizer.h"
#include <iomanip>
#include <cstddef>


constexpr size_t MAX_WORD_NAME_LENGTH = 16;
constexpr size_t WORD_ALIGNMENT = 16;

// Enum to represent the execution state of a Forth word
enum class ForthState {
    EXECUTABLE,
    IMMEDIATE,
    GENERATOR
};

// Enum for Forth word types (extensible)
enum class ForthWordType : uint32_t {
    WORD = 1 << 0,
    CONSTANT = 1 << 1,
    VARIABLE = 1 << 2,
    VALUE = 1 << 3,
    STRING = 1 << 4,
    FLOAT = 1 << 5,
    ARRAY1 = 1 << 6,
    OBJECT = 1 << 7,
    RECORD = 1 << 8,
    ARRAY2 = 1 << 9,
    ARRAY3 = 1 << 10,
    VOCABULARY = 1 << 11,
    MACRO = 1 << 12,

    // Add more types as needed
    // Note: The type flags are stored in the low 32 bits of the ForthWordType enum
    //       to avoid alignment issues with the ForthDictionaryEntry structure
    //       (which is aligned to 16 bytes)
};

// Function pointer types for word execution
using ForthFunction = void(*)();
using ImmediateInterpreter = void(*)(std::deque<ForthToken> &tokens);
using ImmediateCompiler = void(*)(std::deque<ForthToken> &tokens);


struct ForthDictionaryEntry {
    ForthDictionaryEntry *previous;
    // Pointer to the previous word in the dictionary (linked list) - aligned to 16 bytes
    char res1[8]; // Padding to align the next field to 16 bytes
    // Union for fast 64-bit lookup
    union {
        struct {
            uint32_t word_id; // Symbol table ID for the word (4 bytes)
            uint32_t vocab_id; // Symbol table ID for the vocabulary (4 bytes)
        };

        uint64_t id{}; // 64-bit integer for fast comparisons (8 bytes)
    };

    char res2[8];
    ForthState state;
    char res3[12];
    mutable ForthFunction executable; // aligned
    char res4[8];
    ForthFunction generator; // aligned
    char res7[8];
    ImmediateInterpreter immediate_interpreter; // aligned
    char res9[8];
    void *data; // alligned
    ForthDictionaryEntry *firstWordInVocabulary;
    ImmediateCompiler immediate_compiler;
    ForthWordType type;

    // Constructor
    ForthDictionaryEntry(ForthDictionaryEntry *prev, const std::string &wordName,
                         const std::string &vocabName, ForthState wordState, ForthWordType wordType)
        : previous(prev), state(wordState), executable(nullptr), generator(nullptr), immediate_interpreter(nullptr),
          data(nullptr), firstWordInVocabulary(nullptr), immediate_compiler(nullptr), type(wordType) {
        word_id = SymbolTable::instance().addSymbol(wordName);
        vocab_id = SymbolTable::instance().addSymbol(vocabName);
        const char asciiInput[8] = {'F', 'O', 'R', 'T', 'H', 'J', 'I', 'T'};
        std::memcpy(res1, asciiInput, 8);
    }


    ForthDictionaryEntry(ForthDictionaryEntry *prev, const std::string &wordName,
                         const std::string &vocabName, ForthState wordState, ForthWordType wordType,
                         ForthFunction executable)
        : previous(prev), state(wordState), executable(executable), generator(nullptr), immediate_interpreter(nullptr),
          data(nullptr), firstWordInVocabulary(nullptr), immediate_compiler(nullptr), type(wordType) {
        word_id = SymbolTable::instance().addSymbol(wordName);
        vocab_id = SymbolTable::instance().addSymbol(vocabName);
        const char asciiInput[8] = {'F', 'O', 'R', 'T', 'H', 'J', 'I', 'T'};
        std::memcpy(res1, asciiInput, 8);
    }


    ForthDictionaryEntry(ForthDictionaryEntry *prev, const std::string &wordName, const std::string &vocabName,
                         ForthState wordState, ForthWordType wordType, ForthFunction generator,
                         ForthFunction executable, ImmediateInterpreter immediate_interpreter)
        : previous(prev), state(wordState), executable(executable), generator(generator),
          immediate_interpreter(immediate_interpreter),
          data(nullptr), firstWordInVocabulary(nullptr), immediate_compiler(nullptr), type(wordType) {
        word_id = SymbolTable::instance().addSymbol(wordName);
        vocab_id = SymbolTable::instance().addSymbol(vocabName);
        constexpr char asciiInput[8] = {'F', 'O', 'R', 'T', 'H', 'J', 'I', 'T'};
        std::memcpy(res1, asciiInput, 8);
    }

    ForthDictionaryEntry(ForthDictionaryEntry *prev, const std::string &wordName, const std::string &vocabName,
                         ForthState wordState, ForthWordType wordType, ForthFunction generator,
                         ForthFunction executable, ImmediateInterpreter immediate_interpreter,
                         ImmediateCompiler immediate_compiler)
        : previous(prev), state(wordState), executable(executable), generator(generator),
          immediate_interpreter(immediate_interpreter),
          data(nullptr), firstWordInVocabulary(nullptr), immediate_compiler(immediate_compiler), type(wordType) {
        word_id = SymbolTable::instance().addSymbol(wordName);
        vocab_id = SymbolTable::instance().addSymbol(vocabName);
        constexpr char asciiInput[8] = {'F', 'O', 'R', 'T', 'H', 'J', 'I', 'T'};
        std::memcpy(res1, asciiInput, 8);
    }

    bool isAligned16(std::size_t offset) {
        return offset % 16 == 0;
    }

    bool isThisAligned16() const {
        return reinterpret_cast<std::uintptr_t>(this) % 16 == 0;
    }

    // Debug function
    void displayOffsets() {
        std::cout << "Offsets and Alignment Check for ForthDictionaryEntry structure:" << std::endl;
        std::cout << "-------------------------------------------------------------------------" << std::endl;

        // Display the memory address of the current entry
        std::cout << "Memory Address of Entry: "
                << this << " 16 byte Aligned: "
                << (isThisAligned16() ? "Yes" : "No") << std::endl;

        std::cout << "-------------------------------------------------------------------------" << std::endl;

        // Print header
        std::cout << std::setw(30) << "Field"
                << std::setw(15) << "Offset (bytes)"
                << std::setw(20) << "16-Byte Aligned?"
                << std::endl;

        std::cout << "-------------------------------------------------------------------------" << std::endl;

        // Print offsets and alignment status for each member
        auto printMember = [this](const char *name, std::size_t offset) {
            std::cout << std::setw(30) << name
                    << std::setw(15) << offset
                    << std::setw(20) << (isAligned16(offset) ? "Yes" : "No")
                    << std::endl;
        };

        printMember("previous", offsetof(ForthDictionaryEntry, previous));
        printMember("word_id", offsetof(ForthDictionaryEntry, word_id));
        printMember("vocab_id", offsetof(ForthDictionaryEntry, vocab_id));
        printMember("id", offsetof(ForthDictionaryEntry, id));
        printMember("state", offsetof(ForthDictionaryEntry, state));
        printMember("executable", offsetof(ForthDictionaryEntry, executable));
        printMember("immediate_interpreter", offsetof(ForthDictionaryEntry, immediate_interpreter));
        printMember("generator", offsetof(ForthDictionaryEntry, generator));
        printMember("data", offsetof(ForthDictionaryEntry, data));
        printMember("immediate_compiler", offsetof(ForthDictionaryEntry, immediate_compiler));
        printMember("type", offsetof(ForthDictionaryEntry, type));

        std::cout << "-------------------------------------------------------------------------" << std::endl;
    }

    [[nodiscard]] const void *getAddress() const {
        return static_cast<const void *>(this);
    }

    [[nodiscard]] uint64_t getID() const {
        return id;
    }


    // Get the word name from the symbol table
    [[nodiscard]] std::string getWordName() const {
        return SymbolTable::instance().getSymbol(word_id);
    }

    // Get the vocabulary name from the symbol table
    [[nodiscard]] std::string getVocabularyName() const {
        return SymbolTable::instance().getSymbol(vocab_id);
    }


    // Fixed getTypeString function (with proper type casting for enum class ForthWordType)
    [[nodiscard]] std::string getTypeString() const {
        std::string typeStr;

        // Check if the type is an array
        bool isArray = false;

        // Remove array-specific flags to determine the base type
        auto baseType = static_cast<uint32_t>(type); // Cast type to uint32_t
        if (baseType & static_cast<uint32_t>(ForthWordType::ARRAY1)) {
            isArray = true;
            baseType &= ~static_cast<uint32_t>(ForthWordType::ARRAY1);
        }
        if (baseType & static_cast<uint32_t>(ForthWordType::ARRAY2)) {
            isArray = true;
            baseType &= ~static_cast<uint32_t>(ForthWordType::ARRAY2);
        }
        if (baseType & static_cast<uint32_t>(ForthWordType::ARRAY3)) {
            isArray = true;
            baseType &= ~static_cast<uint32_t>(ForthWordType::ARRAY3);
        }

        // Map base types to string names
        if (baseType & static_cast<uint32_t>(ForthWordType::WORD)) typeStr += "WORD ";
        if (baseType & static_cast<uint32_t>(ForthWordType::CONSTANT)) typeStr += "CONSTANT ";
        if (baseType & static_cast<uint32_t>(ForthWordType::VARIABLE)) typeStr += "VARIABLE ";
        if (baseType & static_cast<uint32_t>(ForthWordType::VALUE)) typeStr += "VALUE ";
        if (baseType & static_cast<uint32_t>(ForthWordType::STRING)) typeStr += "STRING ";
        if (baseType & static_cast<uint32_t>(ForthWordType::FLOAT)) typeStr += "FLOAT ";
        if (baseType & static_cast<uint32_t>(ForthWordType::OBJECT)) typeStr += "OBJECT ";
        if (baseType & static_cast<uint32_t>(ForthWordType::RECORD)) typeStr += "RECORD ";
        if (baseType & static_cast<uint32_t>(ForthWordType::VOCABULARY)) typeStr += "VOCABULARY ";
        if (baseType & static_cast<uint32_t>(ForthWordType::MACRO)) typeStr += "MACRO ";

        // Clean up the trailing space
        if (!typeStr.empty() && typeStr.back() == ' ') {
            typeStr.pop_back();
        }

        // Prefix with "ARRAY of" if the type is an array
        if (isArray) {
            typeStr = "ARRAY of " + typeStr;
        }

        // Return the type string or "UNKNOWN" if empty
        return typeStr.empty() ? "UNKNOWN" : typeStr;
    }


    void *AllotData(int n) {
        const auto id = getID();
        data = WordHeap::instance().allocate(id, n);
        return data;
    }

    static void printPointerOrNo(const void* ptr, const char* nullMessage = "No") {
        if (ptr) {
            std::cout << ptr; // Print the pointer address
        } else {
            std::cout << nullMessage; // Print the fallback message
        }
    }

    // Display word details
    void display() const {
        std::cout << "Word: " << getWordName() << "\n";
        std::cout << "  State: "
                << (state == ForthState::EXECUTABLE
                        ? "EXECUTABLE"
                        : state == ForthState::IMMEDIATE
                              ? "IMMEDIATE"
                              : "GENERATOR")
                << "\n";
        std::cout << "  Type: " << getTypeString() << "\n";
        std::cout << "  Data Pointer: ";
        printPointerOrNo((void*)data);
        std::cout << "\n";
        if ( data )
            std::cout << std::dec << "  Data Size: " << (data ? WordHeap::instance().getAllocation(id)->size : 0) << "\n";

        std::cout << "  word_id: " << word_id << "\n";
        std::cout << "  vocabulary: " << SymbolTable::instance().getSymbol(vocab_id) << "\n";
        std::cout << "  ID: " << id << "\n" << std::hex;

        // For each pointer, check whether it's null and display accordingly
        std::cout << "  Previous Word: ";
        if (previous) {
            std::cout << std::hex << previous;
        } else {
            std::cout << "No";
        }
        std::cout << "\n";
        std::cout << std::hex;
        std::cout << "  Executable: ";
        printPointerOrNo((void*)executable); // Cast to void* for safe printing
        std::cout << "\n";

        std::cout << "  Generator: ";
        printPointerOrNo((void*)generator);
        std::cout << "\n";

        std::cout << "  Immediate Function: ";
        printPointerOrNo((void*)immediate_interpreter);
        std::cout << "\n";

        // display data if we have any to display
        if (data) {
            WordHeap::instance().listAllocation(id);
        }
    }
};
#endif // FORTH_DICTIONARY_ENTRY_H
