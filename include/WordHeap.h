#ifndef WORDHEAP_H
#define WORDHEAP_H

#include <unordered_map>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include "Singleton.h"
#include "SymbolTable.h"
#include <iomanip>
#include <cctype>


// Enum to represent the data type of word allocations
enum class WordDataType {
    DEFAULT, // Treat as raw bytes (FORTH behavior)
    BYTE, // Single byte
    INT, // Single integer
    FLOAT, // Single float
    FLOAT_ARRAY, // Array of floats
    STRING // Null-terminated string
    // Add more types as needed
};

// Represents a word's allocated memory and type metadata
class WordHeap : public Singleton<WordHeap> {
    friend class Singleton<WordHeap>;

public:
    struct WordAllocation {
        void *dataPtr; // Pointer to allocated data
        size_t size; // Size of the allocation in bytes
        WordDataType dataType; // Type of data (default: raw bytes)
    };

    // Allocate memory for a word using a 64-bit `id`
    void *allocate(uint64_t wordId, size_t size, WordDataType type = WordDataType::DEFAULT) {
        // If the word already has allocated memory, try to reallocate

        auto it = allocations.find(wordId);
        if (it != allocations.end()) {
            // Handle reallocation
            void *oldData = it->second.dataPtr;
            size_t oldSize = it->second.size;

            // Allocate temporary storage for the old data
            void *tempData = std::malloc(oldSize);
            if (!tempData) {
                std::cerr << "WordHeap: Temporary memory allocation failed for word ID: "
                        << wordId
                        << " Name: " << SymbolTable::instance().getSymbol(wordId)
                        << std::endl;
                return nullptr;
            }

            // Copy the old data to the temporary storage
            std::memcpy(tempData, oldData, oldSize);

            // Attempt to reallocate the memory block to the new size
            void *newPtr = std::realloc(oldData, size);
            if (!newPtr) {
                std::cerr << "WordHeap: Reallocation failed for word ID: " << wordId
                        << " Name: " << SymbolTable::instance().getSymbol(wordId)
                        << ". Restoring original data." << std::endl;

                // Restore the old data if reallocation fails
                std::memcpy(oldData, tempData, oldSize);
                std::free(tempData);
                return nullptr;
            }

            // On successful reallocation, copy the temporary data back (if size is reduced, ensure data integrity)
            std::memcpy(newPtr, tempData, std::min(oldSize, size));
            std::free(tempData);

            // Update allocation metadata
            it->second = {newPtr, size, type}; // Update size and type
            std::cout << "WordHeap: Reallocation succeeded for word ID: " << std::hex << wordId
                    << " Name: " << SymbolTable::instance().getSymbol(wordId)
                    << std::dec << ", new size: " << size << " bytes." << std::endl;
            return newPtr;
        }

        // Allocate new memory: Align the size to 16 bytes
        size = (size + 15) & ~15;
        void *ptr = nullptr;
        if (posix_memalign(&ptr, 16, size) != 0) {
            std::cerr << "WordHeap: Memory allocation failed for word ID: " << wordId
                    << " Name: " << SymbolTable::instance().getSymbol(wordId)
                    << std::endl;
            return nullptr;
        }

        // Store the allocation along with type metadata
        allocations[wordId] = {ptr, size, type};

        std::cout << "WordHeap: Successfully allocated "
                << size << " bytes for word ID: "
                << std::hex << wordId << " Name: " << SymbolTable::instance().getSymbol(wordId)
                << "." << std::endl;
        return ptr;
    }

    // Deallocate a specific word's memory using its ID
    void deallocate(uint64_t wordId) {
        auto it = allocations.find(wordId);
        if (it != allocations.end()) {
            std::free(it->second.dataPtr);
            allocations.erase(it);
            std::cout << "WordHeap: Memory deallocated for word ID: " << wordId << "." << std::endl;
        }
    }

    // Retrieve the allocation metadata for a word using its ID
    WordAllocation *getAllocation(uint64_t wordId) {
        auto it = allocations.find(wordId);
        return (it != allocations.end()) ? &it->second : nullptr;
    }

    void listAllocations() const {
        if (allocations.empty()) {
            std::cout << "WordHeap: No allotments have been allocated." << std::endl;
            return;
        }

        std::cout << "WordHeap: Current allot allocations:" << std::endl;

        for (const auto &[wordId, allocation]: allocations) {
            // Display metadata for the allocation
            std::cout << "  Word ID: " << wordId
                    << ", Name: " << SymbolTable::instance().getSymbol(wordId)
                    << ", Size: " << allocation.size << " bytes"
                    << ", Type: " << wordDataTypeToString(allocation.dataType)
                    << ", Address: " << allocation.dataPtr
                    << ", To: " << reinterpret_cast<void *>((uint64_t) allocation.dataPtr + allocation.size - 1)
                    << std::endl;

            // FORTH people like to work on raw bytes...
            if (allocation.dataType == WordDataType::DEFAULT) {
                // Retrieve pointer to data and calculate the number of bytes to display
                const unsigned char *data = reinterpret_cast<const unsigned char *>(allocation.dataPtr);
                size_t bytesToDisplay = std::min(size_t(32), allocation.size); // Show up to 32 bytes

                // Display the hex ASCII dump
                std::cout << "    Hex ASCII Dump (First " << std::dec << bytesToDisplay << " bytes):" << std::endl;

                // Hex and ASCII formatting loop
                for (size_t i = 0; i < bytesToDisplay; i += 16) {
                    // Group output in 16-byte rows
                    // Print offset
                    std::cout << "      [0x" << std::hex << std::setw(4) << std::setfill('0') << i << "] ";

                    // Print hex values
                    for (size_t j = 0; j < 16; ++j) {
                        if (i + j < bytesToDisplay) {
                            std::cout << std::hex << std::setw(2) << std::setfill('0')
                                    << static_cast<int>(data[i + j]) << " ";
                        } else {
                            std::cout << "   "; // Fills gap for unused bytes
                        }
                    }

                    // Spacer between hex and ASCII
                    std::cout << "  |";

                    // Print ASCII characters
                    for (size_t j = 0; j < 16; ++j) {
                        if (i + j < bytesToDisplay) {
                            unsigned char c = data[i + j];
                            if (std::isprint(c)) {
                                std::cout << c; // Printable characters
                            } else {
                                std::cout << '.'; // Replace unprintable characters with '.'
                            }
                        }
                    }

                    std::cout << "|" << std::endl;
                }
            }
        }
    }

    // Clear all allocations
    void clear() {
        for (auto &alloc: allocations) {
            std::free(alloc.second.dataPtr);
        }
        allocations.clear();
        std::cout << "WordHeap: All allocations cleared." << std::endl;
    }

private:
    WordHeap() = default;

    ~WordHeap() {
        clear(); // Free all memory upon destruction
    }

    // Helper to convert WordDataType to string for display
    const char *wordDataTypeToString(WordDataType type) const {
        switch (type) {
            case WordDataType::DEFAULT: return "Raw Bytes";
            case WordDataType::BYTE: return "Byte";
            case WordDataType::INT: return "Integer";
            case WordDataType::FLOAT: return "Float";
            case WordDataType::FLOAT_ARRAY: return "Float Array";
            case WordDataType::STRING: return "String";
            default: return "Unknown";
        }
    }

    std::unordered_map<uint64_t, WordAllocation> allocations; // Tracks memory allocations using 64-bit word IDs
};

#endif // WORDHEAP_H
