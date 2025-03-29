#ifndef FORTH_DICTIONARY_H
#define FORTH_DICTIONARY_H

#include <memory>      // For std::unique_ptr
#include <string>      // For std::string
#include <vector>      // For std::vector
#include <unordered_map> // For std::unordered_map
#include <array>       // For std::array
#include <Tokenizer.h>

#include "ForthDictionaryEntry.h"
#include "Singleton.h"


// Constants (adjust as needed)

#define MAX_WORD_LENGTH 16

class ForthDictionary : public Singleton<ForthDictionary> {
    friend class Singleton<ForthDictionary>; // Allow Singleton to access private constructor
public:
    // Constructor
    ForthDictionary();

    // Destructor
    ~ForthDictionary();

    ForthDictionaryEntry *addCodeWord(const std::string &wordName, const std::string &vocabName, ForthState wordState,
                                      ForthWordType wordType, ForthFunction generator, ForthFunction executable,
                                      ImmediateInterpreter immediate_interpreter, ImmediateCompiler immediate_compiler);

    ForthDictionaryEntry *addCodeWord(const std::string &wordName,
                                      const std::string &vocabName,
                                      ForthState wordState, ForthWordType wordType, ForthFunction generator,
                                      ForthFunction executable,
                                      ImmediateInterpreter immediate_interpreter);

    // Add a word to the dictionary
    ForthDictionaryEntry* addWord(const char* name, ForthState state, ForthWordType type, const std::string& vocabName);

    // Create a new vocabulary
    ForthDictionaryEntry* createVocabulary(const std::string &vocabName);

    void displayWordChain(size_t length) const;

    std::string getCurrentVocabularyName();

    ForthDictionaryEntry *getLatestWordAdded() const;

    std::string getLatestName();

    // Find a word in the dictionary using the search order
    ForthDictionaryEntry* findWord(const char* name) const;

    bool isVariable(const char *name) const;

    void execWord(const char *name);

    ForthDictionaryEntry *findWordByToken(const ForthToken &word) const;



    ForthDictionaryEntry *findWordById(uint32_t word_id) const;

    void execWordByToken(const ForthToken &word) const;

    ForthDictionaryEntry *findVocab(const char *name);

    // Set the active vocabulary
    void setVocabulary(const std::string& vocabName);

    void setVocabulary(ForthDictionaryEntry *vocab);


    // Set search order (list of vocabularies)
    void setSearchOrder(const std::vector<std::string>& order);

    // Add a vocabulary to the search order
    void addSearchOrder(const std::string& vocabName);

    // Remove all vocabularies from the search order
    void resetSearchOrder();

    // Remove a word from the dictionary
    void removeWord(const char* name);

    // Display the dictionary contents
    void displayDictionary() const;

    void displayWords() const;

    void forgetLastWord();

private:
    void addToCache(const std::string &name, ForthDictionaryEntry *entry);



    ForthDictionaryEntry* findInCache(const std::string &name) const;

private:
    // Dictionary lists (by word length): manage entries using smart pointers
    std::array<ForthDictionaryEntry*, MAX_WORD_LENGTH> dictionaryLists{};

    // The currently active vocabulary
    ForthDictionaryEntry* currentVocabulary{};

    // The search order for vocabularies
    std::vector<ForthDictionaryEntry*> searchOrder;

    // Mapping from vocabulary name to its entry
    std::unordered_map<std::string, ForthDictionaryEntry*> vocabularies;

    std::vector<std::unique_ptr<ForthDictionaryEntry>> unusedVocabularyStorage;

    ForthDictionaryEntry *latestWordAdded{};
    mutable ForthDictionaryEntry *latestWordFound{};
    ForthDictionaryEntry *latestWordExecuted{};
    ForthDictionaryEntry *latestVocabAdded{};
    ForthDictionaryEntry *latestVocabFound{};
    std::string latestWordName;
    std::vector<ForthDictionaryEntry*> wordOrder;
    struct WordCacheEntry {
        std::string name;
        ForthDictionaryEntry *entry; // Pointer to the actual dictionary entry
    };
    std::vector<WordCacheEntry> wordCache; // Cache

};

#endif // FORTH_DICTIONARY_H