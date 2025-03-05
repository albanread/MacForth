#include <unordered_set>
#include <string>
#include <mutex>
#include <memory>
#include <iostream>
#include <cstdlib> // For aligned_alloc (C++17)

class StringStorage {
public:
    static StringStorage& instance() {
        static StringStorage instance;
        return instance;
    }

    // Returns an interned string with guaranteed 16-byte alignment
    const char* intern(const std::string& str);

    // Clears all interned strings
    void clear();

    // Displays all interned strings along with their addresses
    void displayInternedStrings() const;

private:
    StringStorage() = default;
    ~StringStorage();

    // Aligned string pool
    std::unordered_set<const char*> internedStrings;

    // Mutex for thread safety
    mutable std::mutex mutex;

    // Custom deallocation function for aligned memory
    void freeAligned(const char* ptr);
};

inline const char* StringStorage::intern(const std::string& str) {
    std::lock_guard<std::mutex> lock(mutex);

    // Check if the string is already interned
    for (const char* s : internedStrings) {
        if (strcmp(s, str.c_str()) == 0) {
            return s; // Return the existing interned string if it matches
        }
    }

    // Allocate aligned memory for the new string
    size_t size = str.size() + 1; // Include space for null terminator
    char* alignedString = static_cast<char*>(std::aligned_alloc(16, ((size + 15) / 16) * 16));
    if (!alignedString) {
        throw std::bad_alloc();
    }

    // Copy the string into the aligned memory
    std::strncpy(alignedString, str.c_str(), size);

    // Insert the aligned string into the pool
    internedStrings.insert(alignedString);

    return alignedString;
}

inline void StringStorage::clear() {
    std::lock_guard<std::mutex> lock(mutex);

    // Free all aligned memory strings
    for (const char* s : internedStrings) {
        freeAligned(s);
    }

    internedStrings.clear();
}

inline void StringStorage::displayInternedStrings() const {
    std::lock_guard<std::mutex> lock(mutex);

    std::cout << "Interned Strings:\n";
    for (const char* s : internedStrings) {
        std::cout << "String: [" << s << "] Address: " << std::hex
                  << reinterpret_cast<const void*>(s) << std::dec << "\n";
    }
}

inline StringStorage::~StringStorage() {
    // Free all memory on destruction
    clear();
}

inline void StringStorage::freeAligned(const char* ptr) {
    // Free aligned memory
    std::free(const_cast<char*>(ptr));
}