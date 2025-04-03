#include "ForthDictionary.h"
#include <cstring>
#include <iostream>
#include <JitContext.h>
#include <unordered_set>
#include <asmjit/core/jitruntime.h>

#include "Quit.h"
#include "SymbolTable.h"
#include "Tokenizer.h"
#include "SignalHandler.h"
#include <map>

// Define a helper function to retrieve color codes
std::string getColorCode(ForthWordType type) {
    static const std::map<ForthWordType, std::string> colorMap = {
        {ForthWordType::CONSTANT, "\033[1;31m"}, // Red
        {ForthWordType::WORD, "\033[1;32m"}, // Green
        {ForthWordType::VARIABLE, "\033[1;34m"}, // Blue
        {ForthWordType::VALUE, "\033[1;33m"}, // Yellow
        {ForthWordType::STRING, "\033[1;35m"}, // Magenta
        {ForthWordType::FLOAT, "\033[1;36m"}, // Cyan
        {ForthWordType::ARRAY1, "\033[0;31m"}, // Dim Red
        {ForthWordType::MACRO, "\033[0;32m"}, // Dim Green
        {ForthWordType::RECORD, "\033[0;34m"}, // Dim Blue
        {ForthWordType::ARRAY2, "\033[0;33m"}, // Dim Yellow
        {ForthWordType::ARRAY3, "\033[0;35m"}, // Dim Magenta
        {ForthWordType::OBJECT, "\033[0;36m"}, // Dim Cyan
        {ForthWordType::VOCABULARY, "\033[1;37m"}, // Bright White
    };

    auto it = colorMap.find(type);
    if (it != colorMap.end()) {
        return it->second;
    }
    return "\033[0m"; // Default (reset) if type is unknown
}

ForthDictionary::ForthDictionary() {
    // Initialize all dictionary lists to null pointers
    for (auto &entry: dictionaryLists) {
        entry = nullptr;
    }

    // Create and set up the default vocabulary "FORTH"
}

ForthDictionary::~ForthDictionary() {
    // Free all allocated entries in all dictionary lists
    for (size_t i = 0; i < MAX_WORD_LENGTH; ++i) {
        ForthDictionaryEntry *current = dictionaryLists[i];
        while (current) {
            ForthDictionaryEntry *toDelete = current;
            current = current->previous;
            delete toDelete; // Free each link in the chain
        }
        dictionaryLists[i] = nullptr;
    }

    // Clear the vocabularies map (no need to delete the entries, as they're handled above)
    vocabularies.clear();
}

ForthDictionaryEntry *ForthDictionary::addCodeWord(
    const std::string &wordName,
    const std::string &vocabName,
    const ForthState wordState,
    const ForthWordType wordType,
    const ForthFunction generator,
    const ForthFunction executable,
    const ImmediateInterpreter immediate_interpreter,
    const ImmediateCompiler immediate_compiler
) {
    const size_t length = wordName.size();
    if (length >= MAX_WORD_LENGTH) {
        throw std::length_error("Word length exceeds the maximum allowed size.");
    }

    // Get the current head of the list for this word length
    ForthDictionaryEntry *oldHead = dictionaryLists[length]; // Current head of the chain

    std::string vocab_name = vocabName;

    if (vocabName.empty()) {
        vocab_name = getCurrentVocabularyName();
    }

    // uppercase the vocab_name
    std::string vocab_nameStr(vocab_name);
    std::transform(vocab_nameStr.begin(), vocab_nameStr.end(), vocab_nameStr.begin(), ::toupper);

    // uppercase the wordName
    std::string wordNameStr(wordName);
    std::transform(wordNameStr.begin(), wordNameStr.end(), wordNameStr.begin(), ::toupper);


    void *memory = std::aligned_alloc(16, sizeof(ForthDictionaryEntry));
    if (!memory) {
        throw std::bad_alloc{};
    }
    const auto newWord = new(memory) ForthDictionaryEntry(oldHead,
                                                          wordNameStr, vocab_nameStr,
                                                          wordState, wordType,
                                                          generator,
                                                          executable,
                                                          immediate_interpreter,
                                                          immediate_compiler);

    // Update the head of the list for this word length
    dictionaryLists[length] = newWord;
    latestWordAdded = newWord; // Update the latest word
    latestWordName = wordName; // Update the latest word name
    wordOrder.push_back(newWord); // Track addition order
    return newWord; // Return the newly added entry
}


ForthDictionaryEntry *ForthDictionary::addCodeWord(
    const std::string &wordName,
    const std::string &vocabName,
    const ForthState wordState,
    const ForthWordType wordType,
    const ForthFunction generator,
    const ForthFunction executable,
    const ImmediateInterpreter immediate_interpreter) {
    const size_t length = wordName.size();
    if (length >= MAX_WORD_LENGTH) {
        throw std::length_error("Word length exceeds the maximum allowed size.");
    }

    // Get the current head of the list for this word length
    ForthDictionaryEntry *oldHead = dictionaryLists[length]; // Current head of the chain

    std::string vocab_name = vocabName;

    if (vocabName.empty()) {
        vocab_name = getCurrentVocabularyName();
    }

    // uppercase the vocab_name
    std::string vocab_nameStr(vocab_name);
    std::transform(vocab_nameStr.begin(), vocab_nameStr.end(), vocab_nameStr.begin(), ::toupper);

    // uppercase the wordName
    std::string wordNameStr(wordName);
    std::transform(wordNameStr.begin(), wordNameStr.end(), wordNameStr.begin(), ::toupper);


    void *memory = std::aligned_alloc(16, sizeof(ForthDictionaryEntry));
    if (!memory) {
        throw std::bad_alloc{};
    }
    const auto newWord = new(memory) ForthDictionaryEntry(oldHead,
                                                          wordNameStr, vocab_nameStr,
                                                          wordState, wordType,
                                                          generator,
                                                          executable,
                                                          immediate_interpreter);

    // Update the head of the list for this word length
    dictionaryLists[length] = newWord;
    latestWordAdded = newWord; // Update the latest word
    latestWordName = wordName; // Update the latest word name
    wordOrder.push_back(newWord); // Track addition order
    return newWord; // Return the newly added entry
}

ForthDictionaryEntry *ForthDictionary::findWord(const char *name) const {
    if (!name) {
        throw std::invalid_argument("Name cannot be null!");
    }

    // uppercase the name
    std::string nameStr(name);
    std::transform(nameStr.begin(), nameStr.end(), nameStr.begin(), ::toupper);
    name = nameStr.c_str();


    size_t length = std::strlen(name);
    if (length >= MAX_WORD_LENGTH) {
        return nullptr; // Word is too long, invalid
    }

    auto word_id = SymbolTable::instance().addSymbol(name);

    // Create a set of vocab IDs prioritized by search order
    std::unordered_set<size_t> vocabSet;
    for (const auto &vocab: searchOrder) {
        if (vocab != nullptr) {
            vocabSet.insert(vocab->vocab_id);
        }
    }

    // Iterate over words of the same length
    ForthDictionaryEntry *current = dictionaryLists[length];
    while (current) {
        // Check both word ID and vocab ID
        if (current->word_id == word_id && vocabSet.count(current->vocab_id) > 0) {
            latestWordFound = current; // Update latest found word
            return current;
        }
        current = current->previous;
    }

    return nullptr; // Word not found
}


bool ForthDictionary::isVariable(const char *name) const {
    if (!name) {
        throw std::invalid_argument("Name cannot be null!");
    }

    // uppercase the name
    std::string nameStr(name);
    std::transform(nameStr.begin(), nameStr.end(), nameStr.begin(), ::toupper);
    name = nameStr.c_str();

    size_t length = std::strlen(name);
    if (length >= MAX_WORD_LENGTH) {
        return false; // Word is too long, invalid
    }

    auto word_id = SymbolTable::instance().findSymbol(name);
    if (word_id == 0) return false;

    // Create a set of vocab IDs prioritized by search order
    std::unordered_set<size_t> vocabSet;
    for (const auto &vocab: searchOrder) {
        if (vocab != nullptr) {
            vocabSet.insert(vocab->vocab_id);
        }
    }

    // Iterate over words of the same length
    ForthDictionaryEntry *current = dictionaryLists[length];
    while (current) {
        // Check both word ID and vocab ID
        if (current->word_id == word_id && vocabSet.count(current->vocab_id) > 0) {
            return (current->type == ForthWordType::VARIABLE);
        }
        current = current->previous;
    }

    return false; // Word not found
}


// this executes 'simple' executable names.
void ForthDictionary::execWord(const char *name) {
    auto word = findWord(name);
    if (word == nullptr) {
        throw std::invalid_argument("Word not found!");
    }
    if (word->state != ForthState::EXECUTABLE) {
        throw std::invalid_argument("Word is not executable!");
    }
    if (word->executable == nullptr) {
        throw std::invalid_argument("Word has no executable function!");
    }
    latestWordExecuted = word;
    word->executable();
}


ForthDictionaryEntry *ForthDictionary::findWordByToken(const ForthToken &word) const {
    if (word.word_len >= MAX_WORD_LENGTH) {
        return nullptr; // Word length is invalid
    }

    // Create a set of vocab IDs from the search order for fast lookup
    std::unordered_set<size_t> vocabSet;
    for (const auto &vocab: searchOrder) {
        if (vocab != nullptr) {
            vocabSet.insert(vocab->vocab_id);
        }
    }

    // Traverse the linked list for the given word length
    ForthDictionaryEntry *current = dictionaryLists[word.word_len];
    while (current) {
        // Check for both vocab ID and word ID match
        if (vocabSet.count(current->vocab_id) > 0 && current->word_id == word.word_id) {
            latestWordFound = current; // Update latest found word
            return current; // Found the word
        }
        current = current->previous;
    }

    return nullptr; // Word not found
}


void ForthDictionary::execWordByToken(const ForthToken &word) const {
    auto found_word = findWordByToken(word);

    if (!found_word) {
        SignalHandler::instance().raise(5);
    }

    found_word->executable();
}

ForthDictionaryEntry *ForthDictionary::addWord(const char *name, ForthState state, ForthWordType type,
                                               const std::string &vocabName) {
    if (!name || vocabName.empty()) {
        throw std::invalid_argument("Name or vocabulary cannot be empty!");
    }

    // uppercase the name
    std::string nameStr(name);
    std::transform(nameStr.begin(), nameStr.end(), nameStr.begin(), ::toupper);
    name = nameStr.c_str();

    // uppercase the vocab name
    std::string vocabNameStr(vocabName);
    std::transform(vocabNameStr.begin(), vocabNameStr.end(), vocabNameStr.begin(), ::toupper);


    size_t length = std::strlen(name);
    if (length >= MAX_WORD_LENGTH) {
        throw std::length_error("Word length exceeds the maximum allowed size.");
    }

    // Get the current head of the list for this word length
    ForthDictionaryEntry *oldHead = dictionaryLists[length]; // Current head of the chain

    ForthDictionaryEntry *vocab = findVocab(vocabName.c_str());
    if (!vocab) {
        throw std::invalid_argument("Vocabulary not found!");
    }

    void *memory = std::aligned_alloc(16, sizeof(ForthDictionaryEntry));
    if (!memory) {
        throw std::bad_alloc{};
    }

    auto *newWord = new(memory) ForthDictionaryEntry(oldHead, name, vocabName, state, type);


    // Update the head of the list for this word length
    dictionaryLists[length] = newWord;

    latestWordAdded = newWord; // Update the latest word

    wordOrder.push_back(newWord); // Track addition order
    return newWord; // Return the newly added entry
}


ForthDictionaryEntry *ForthDictionary::findVocab(const char *name) {
    if (!name) {
        throw std::invalid_argument("Name cannot be null!");
    }
    size_t length = std::strlen(name);
    if (length >= MAX_WORD_LENGTH) {
        return nullptr; // Word is too long, invalid
    }
    auto vocab_id = SymbolTable::instance().addSymbol(name);
    ForthDictionaryEntry *current = dictionaryLists[length]; // Start with the chain for the correct word length
    while (current) {
        // Compare word name and vocabulary address to find the match
        if (current->vocab_id == vocab_id) {
            latestVocabFound = current; // Update the latest vocabulary
            return current; // Found the word
        }
        current = current->previous; // Traverse the chain backward
    }
    return nullptr; // Word not found
}


void ForthDictionary::displayDictionary() const {
    std::cout << "Forth Dictionary (Current Vocabulary: " << currentVocabulary << ")\n";
    for (size_t i = 0; i < MAX_WORD_LENGTH; ++i) {
        ForthDictionaryEntry *current = dictionaryLists[i];
        while (current) {
            current->display(); // Call the display method for each entry
            current = current->previous;
        }
    }
}


void ForthDictionary::displayWords() const {
    std::string vocab_name = instance().getCurrentVocabularyName();
    std::cout << "Forth Dictionary (Current Vocabulary: " << vocab_name << ")\n";
    std::cout << "LatestWord: " << latestWordName << "\n";

    int n = 0;
    for (auto* entry : wordOrder) {
        // Get the color code for the word type
        std::string color = getColorCode(entry->type);

        // UNSAFE words
        if (entry->vocab_id == 3) {
            color = "\033[1;31m"; // RED
        }

        // Print the word with its corresponding color
        std::cout << color << entry->getWordName() << "\033[0m "; // Reset to default after printing

        n += entry->getWordName().size();

        // Wrap the output to avoid overly long lines
        if (n > 44) {
            std::cout << std::endl;
            n = 0;
        }
    }

    // Ensure the output stream is flushed
    std::cout << std::endl << std::flush;
}



void ForthDictionary::setVocabulary(const std::string &vocabName) {
    if (!findVocab(vocabName.c_str())) {
        throw std::invalid_argument("Vocabulary " + vocabName + " does not exist.");
    }
    currentVocabulary = findWord(vocabName.c_str());
    // std::cout << "Current vocabulary set to: " << vocabName << "\n";
}

void ForthDictionary::setVocabulary(ForthDictionaryEntry *vocab) {
    currentVocabulary = vocab;
    // std::cout << "Current vocabulary set to: " << SymbolTable::instance().getSymbol(currentVocabulary->id) << "\n";
}


void ForthDictionary::setSearchOrder(const std::vector<std::string> &order) {
    searchOrder.clear();
    for (const auto &vocabName: order) {
        if (!findVocab(vocabName.c_str())) {
            printf("Vocabulary %s does not exist. Creating it...\n", vocabName.c_str());
            createVocabulary(vocabName);
        }
        ForthDictionaryEntry *vocab = findVocab(vocabName.c_str());
        searchOrder.push_back(vocab); // Add pointers to vocabularies to the search order
    }
}

void ForthDictionary::addSearchOrder(const std::string &vocabName) {
    ForthDictionaryEntry *vocab = findWord(vocabName.c_str());
    if (!vocab) {
        throw std::invalid_argument("Vocabulary " + vocabName + " does not exist.");
    }
    if (std::find(searchOrder.begin(), searchOrder.end(), vocab) == searchOrder.end()) {
        searchOrder.push_back(vocab); // Add to the search order if not already present
    } else {
        throw std::logic_error("Vocabulary already exists in the search order.");
    }
}

void ForthDictionary::resetSearchOrder() {
    searchOrder.clear();
    searchOrder.push_back(findWord("FORTH"));
}


ForthDictionaryEntry *ForthDictionary::createVocabulary(const std::string &vocabName) {
    if (vocabName.empty()) {
        throw std::invalid_argument("Vocabulary name cannot be empty.");
    }

    if (findVocab(vocabName.c_str())) {
        return findVocab(vocabName.c_str());
    }

    auto length = strlen(vocabName.c_str());
    // Get the current head of the list for this word length
    ForthDictionaryEntry *oldHead = dictionaryLists[length]; // Current head of the chain


    // Allocate a new entry and link it
    auto *vocabEntry = new ForthDictionaryEntry(
        oldHead, // Link to the previous word
        vocabName, // Word name
        vocabName, // Vocabulary
        ForthState::EXECUTABLE, // Word state
        ForthWordType::VOCABULARY // Word type
    );

    //vocabEntry->display();

    // Update the head of the list for this word length
    dictionaryLists[length] = vocabEntry;

    // Add the vocabulary to the map
    vocabularies[vocabName] = vocabEntry;

    // Add the vocabulary entry to the dictionary list

    if (length >= MAX_WORD_LENGTH) {
        length = MAX_WORD_LENGTH - 1; // Clamp length to max allowed value
    }

    vocabEntry->previous = oldHead; // Link to previous entry in the list
    dictionaryLists[length] = vocabEntry; // Update the head of the list

    // Automatically add to the search order
    if (std::find(searchOrder.begin(), searchOrder.end(), findWord(vocabName.c_str())) == searchOrder.end()) {
        searchOrder.push_back(findWord(vocabName.c_str()));
    }

    // std::cout << "Created vocabulary: " << vocabName << "\n";

    return vocabEntry; // Return the newly created vocabulary
}

void ForthDictionary::displayWordChain(const size_t length) const {
    if (length >= MAX_WORD_LENGTH) {
        std::cout << "Invalid length!\n";
        return;
    }

    std::cout << "Word Chain for length " << length << ":\n";
    ForthDictionaryEntry *current = dictionaryLists[length];
    while (current) {
        std::cout << "  - " << SymbolTable::instance().getSymbol(current->word_id)
                << " (vocab: " << SymbolTable::instance().getSymbol(current->vocab_id) << ")\n";
        current = current->previous;
    }
    std::cout << "End of chain\n";
}

std::string ForthDictionary::getCurrentVocabularyName() {
    if (!currentVocabulary) {
        currentVocabulary = findVocab("FORTH");
    }
    return {SymbolTable::instance().getSymbol(currentVocabulary->vocab_id)};
}

//
ForthDictionaryEntry *ForthDictionary::getLatestWordAdded() const {
    return latestWordAdded;
}

// get latest WordName
std::string ForthDictionary::getLatestName() {
    return latestWordName;
}

ForthDictionaryEntry *ForthDictionary::findInCache(const std::string &name) const {
    for (const auto &cachedEntry: wordCache) {
        if (cachedEntry.name == name) {
            return cachedEntry.entry; // Found in cache
        }
    }
    return nullptr; // Not found in cache
}

void ForthDictionary::addToCache(const std::string &name, ForthDictionaryEntry *entry) {
    // If cache already has 4 items, remove the oldest one (FIFO)
    if (wordCache.size() >= 4) {
        wordCache.erase(wordCache.begin()); // Remove the first element
    }
    // Add the new entry to the cache
    wordCache.push_back({name, entry});
}

void ForthDictionary::forgetLastWord() {
    if (wordOrder.empty()) {
        std::cerr << "Error: No word to forget.\n";
        return;
    }

    // The word to forget is the most recently added one
    ForthDictionaryEntry *wordToForget = wordOrder.back();
    wordOrder.pop_back();

    std::cout << "Forgetting word: " << latestWordName << "\n";

    const size_t length = latestWordName.size();

    if (wordToForget->executable) {
        // free asmjit memory
        if (const auto runtime = &JitContext::instance()._rt) {
            runtime->release(wordToForget->executable);
            wordToForget->executable = nullptr;
            runtime->release(wordToForget->immediate_interpreter);
            wordToForget->immediate_interpreter = nullptr;
            runtime->release(wordToForget->generator);
            wordToForget->generator = nullptr;
            runtime->release(wordToForget->immediate_compiler);
            wordToForget->immediate_compiler = nullptr;
        }
    }


    // Free any associated memory from WordHeap
    WordHeap::instance().deallocate(wordToForget->word_id);

    // Update dictionaryLists to remove the entry
    auto removeFromChain = [](ForthDictionaryEntry *&head, ForthDictionaryEntry *entry) {
        if (head == entry) {
            head = entry->previous;
            return true;
        }
        ForthDictionaryEntry *current = head;
        while (current && current->previous != entry) {
            current = current->previous;
        }
        if (current) {
            current->previous = entry->previous;
            return true;
        }
        return false;
    };

    if (!removeFromChain(dictionaryLists[length], wordToForget)) {
        std::cerr << "Error: Word not found in dictionary lists.\n";
    }

    // Forget the word's name from the SymbolTable
    SymbolTable::instance().forgetSymbol(latestWordName);

    // Update the latest word
    if (!wordOrder.empty()) {
        latestWordAdded = wordOrder.back();
        latestWordName = SymbolTable::instance().getSymbol(latestWordAdded->word_id);
    } else {
        latestWordAdded = nullptr;
        latestWordName.clear();
    }

    // Remove from unusedVocabularyStorage
    unusedVocabularyStorage.erase(
        std::remove_if(unusedVocabularyStorage.begin(),
                       unusedVocabularyStorage.end(),
                       [wordToForget](const std::unique_ptr<ForthDictionaryEntry> &entry) {
                           return entry.get() == wordToForget;
                       }),
        unusedVocabularyStorage.end());

    // Finally, delete the word itself
    delete wordToForget;
}
