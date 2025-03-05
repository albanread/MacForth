#include <CodeGenerator.h>
#include <gtest/gtest.h>
#include "ForthDictionary.h"


TEST(ForthDictionaryTest, AddWordsAndChain) {

    ForthDictionary& dict = ForthDictionary::instance();
    dict.setVocabulary("FORTH");
    // Add words
    ForthDictionaryEntry* word1 = dict.addWord("TEST1", ForthState::EXECUTABLE, ForthWordType::WORD, "FORTH");
    ForthDictionaryEntry* word2 = dict.addWord("TEST2", ForthState::EXECUTABLE, ForthWordType::WORD, "FORTH");

    // Ensure words are valid
    ASSERT_NE(word1, nullptr);
    ASSERT_NE(word2, nullptr);

    // Verify that "TEST2" points to "TEST1"
    EXPECT_EQ(word2->previous, word1);

    dict.displayWordChain(5);

    // Verify chain traversal
    ForthDictionaryEntry* foundWord = dict.findWord("TEST1");
    EXPECT_EQ(foundWord, word1);

    foundWord = dict.findWord("TEST2");
    EXPECT_EQ(foundWord, word2);
}

// Test adding words to the dictionary
TEST(ForthDictionaryTest, AddWords) {
    ForthDictionary& dict = ForthDictionary::instance();

    dict.addWord("TEST1", ForthState::EXECUTABLE, ForthWordType::WORD, "FORTH");
    dict.addWord("TEST2", ForthState::IMMEDIATE, ForthWordType::CONSTANT, "FORTH");
    dict.displayDictionary();

    EXPECT_NE(dict.findWord("TEST1"), nullptr);
    EXPECT_NE(dict.findWord("TEST2"), nullptr);
    EXPECT_EQ(dict.findWord("UNKNOWN"), nullptr);
}

// Test vocabulary creation and switching
// TEST(ForthDictionaryTest, VocabularyManagement) {
//     ForthDictionary& dict = ForthDictionary::instance();
//     dict.createVocabulary("MATH");
//
//     dict.setVocabulary("MATH");
//
//     dict.addWord("SIN", ForthState::EXECUTABLE, ForthWordType::WORD, "MATH");
//     dict.addWord("COS", ForthState::EXECUTABLE, ForthWordType::WORD, "MATH");
//     dict.setVocabulary("FORTH");
//
//     EXPECT_EQ(dict.findWord("SIN"), nullptr);
//     EXPECT_EQ(dict.findWord("COS"), nullptr);
//     EXPECT_EQ(dict.findWord("LOG"), nullptr);
//
// }

// Test vocabulary search order
TEST(ForthDictionaryTest, VocabularySearchOrder) {
    ForthDictionary& dict = ForthDictionary::instance();
    dict.createVocabulary("STRING");
    dict.setVocabulary("STRING");

    dict.addWord("UPPERCASE", ForthState::EXECUTABLE, ForthWordType::WORD, "STRING");
    dict.addWord("LOWERCASE", ForthState::EXECUTABLE, ForthWordType::WORD, "STRING");

    dict.setSearchOrder({"FORTH", "MATH", "STRING"});
    EXPECT_NE(dict.findWord("UPPERCASE"), nullptr);
    EXPECT_NE(dict.findWord("SIN"), nullptr);
    EXPECT_EQ(dict.findWord("UNKNOWN"), nullptr);
}

// Test clearing search order (ONLY)
TEST(ForthDictionaryTest, ResetSearchOrder) {
    ForthDictionary& dict = ForthDictionary::instance();
    dict.setVocabulary("FORTH");
    dict.resetSearchOrder();
    EXPECT_EQ(dict.getCurrentVocabularyName(), "FORTH");
}



// Test adding and finding words with special or invalid characters
TEST(ForthDictionaryTest, SpecialCharactersInWords) {
    ForthDictionary& dict = ForthDictionary::instance();


    dict.setVocabulary("FORTH");

    // Add words with special characters
    ForthDictionaryEntry* word1 = dict.addWord("WORD_1", ForthState::EXECUTABLE, ForthWordType::WORD, "FORTH");
    ForthDictionaryEntry* word2 = dict.addWord("WORD-2", ForthState::EXECUTABLE, ForthWordType::WORD, "FORTH");
    ForthDictionaryEntry* word3 = dict.addWord("WORD$3", ForthState::EXECUTABLE, ForthWordType::WORD, "FORTH");

    // Verify that words with special characters exist
    EXPECT_NE(dict.findWord("WORD_1"), word1);
    EXPECT_NE(dict.findWord("WORD-2"), word2);
    EXPECT_NE(dict.findWord("WORD$3"), word3);

    // Ensure invalid names are handled gracefully
    EXPECT_EQ(dict.findWord("INVALID@WORD"), nullptr);
}

// Test empty dictionary behavior
TEST(ForthDictionaryTest, EmptyDictionary) {
    ForthDictionary& dict = ForthDictionary::instance();

    dict.createVocabulary("EMPTY");
    dict.setVocabulary("EMPTY");

    // Verify that no words exist in the freshly created vocabulary
    EXPECT_EQ(dict.findWord("ANY"), nullptr);
    EXPECT_EQ(dict.findWord("WORD"), nullptr);
}

// Test setting a search order with an empty vocabulary
TEST(ForthDictionaryTest, SearchOrderWithEmptyVocabulary) {
    ForthDictionary& dict = ForthDictionary::instance();

    dict.createVocabulary("EMPTY");
    dict.setVocabulary("EMPTY");
    dict.resetSearchOrder(); // Reset search order to include only EMPTY vocab
    dict.setSearchOrder({"EMPTY"});

    // Add no words to "EMPTY" vocabulary
    EXPECT_EQ(dict.findWord("NOTHING"), nullptr); // Verify no words are found even with empty vocab in search order
}

// Test for invalid input when searching (null pointers, invalid words)
TEST(ForthDictionaryTest, InvalidInputSearch) {
    ForthDictionary& dict = ForthDictionary::instance();

    EXPECT_THROW(dict.findWord(nullptr), std::invalid_argument); // Searching with null name should throw an exception
    EXPECT_EQ(dict.findWord(""), nullptr);                      // Searching with an empty string should return nullptr
}

// Test chain functionality across multiple vocabularies (with identical word names)
TEST(ForthDictionaryTest, MultipleVocabulariesWithSameWord) {
    ForthDictionary& dict = ForthDictionary::instance();

    dict.createVocabulary("VOCAB1");
    dict.createVocabulary("VOCAB2");

    // Add words to separate vocabularies
    dict.setVocabulary("VOCAB1");
    ForthDictionaryEntry* vocab1Word = dict.addWord("SHARED", ForthState::EXECUTABLE, ForthWordType::WORD, "VOCAB1");

    dict.setVocabulary("VOCAB2");
    ForthDictionaryEntry* vocab2Word = dict.addWord("SHARED", ForthState::EXECUTABLE, ForthWordType::WORD, "VOCAB2");

    // Ensure both words exist
    ASSERT_NE(vocab1Word, nullptr);
    ASSERT_NE(vocab2Word, nullptr);

    // Verify search respects vocabulary order
    dict.setSearchOrder({"VOCAB2", "VOCAB1"});

    ForthDictionaryEntry* foundWord = dict.findWord("SHARED");
    EXPECT_EQ(foundWord, vocab2Word);  // Word from VOCAB2 should be found first

    dict.setSearchOrder({"VOCAB1"});
    foundWord = dict.findWord("SHARED");
    EXPECT_EQ(foundWord, vocab1Word);  // Now it should find the word from VOCAB1
}

#include <random>
#include <string>
#include <unordered_set>
#include <vector>
#include <algorithm>


// Helper function to generate a single random string
std::string generateRandomString(size_t minLength, size_t maxLength) {
    const std::string characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::string randomStr;
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> lengthDistribution(minLength, maxLength);
    std::uniform_int_distribution<> charDistribution(0, characters.size() - 1);

    size_t length = lengthDistribution(generator);
    for (size_t i = 0; i < length; ++i) {
        randomStr += characters[charDistribution(generator)];
    }
    return randomStr;
}

// Function to generate 250,025 unique random names
std::vector<std::string> generateUniqueRandomNames(size_t numNames, size_t minLength, size_t maxLength) {
    std::unordered_set<std::string> uniqueNames; // Ensures uniqueness
    std::vector<std::string> result;

    while (uniqueNames.size() < numNames) {
        std::string randomName = generateRandomString(minLength, maxLength);
        if (uniqueNames.insert(randomName).second) { // Add to set
            result.push_back(randomName);           // Add to result vector
        }
    }
    return result;
}

TEST(ForthDictionaryTest, HighVolumeStressTest) {
    ForthDictionary& dict = ForthDictionary::instance();


    // Step 1: Generate 25025 unique random names of random lengths between 1 and 15
    const size_t totalUniqueNames = 25025;
    const size_t minWordOrVocabLength = 1;
    const size_t maxWordOrVocabLength = 15;
    std::vector<std::string> uniqueNames = generateUniqueRandomNames(
        totalUniqueNames, minWordOrVocabLength, maxWordOrVocabLength);

    // Step 2: Randomly select 25 names for vocabularies from uniqueNames and remove them
    const size_t numVocabularies = 25;
    std::random_device rd;
    std::mt19937 generator(rd());
    std::shuffle(uniqueNames.begin(), uniqueNames.end(), generator); // Shuffle the name pool

    std::vector<std::string> vocabNames(uniqueNames.begin(), uniqueNames.begin() + numVocabularies);
    uniqueNames.erase(uniqueNames.begin(), uniqueNames.begin() + numVocabularies); // Remove used vocab names

    for (const auto& vocabName : vocabNames) {
        dict.createVocabulary(vocabName);
    }

    constexpr size_t numWords = 25000;
    std::unordered_map<std::string, std::vector<std::string>> vocabWords; // Track words in each vocab

    std::uniform_int_distribution<> vocabIndexDistribution(0, vocabNames.size() - 1); // Random vocab index

    for (size_t i = 0; i < numWords; ++i) {
        // Randomly select a vocabulary from the vocabNames list
        const std::string& selectedVocab = vocabNames[vocabIndexDistribution(generator)];

        // Randomly select a word name from uniqueNames and remove it from the pool
        if (uniqueNames.empty()) {
            FAIL() << "Not enough unique names available to continue the test.";
        }

        std::string wordName = uniqueNames.back(); // Take the last name
        uniqueNames.pop_back(); // Remove it from the pool

        // Debug: Print the size after popping
        //printf("Unique names remaining: %zu\n", uniqueNames.size());

        // Set the dictionary to the selected vocabulary
        dict.setVocabulary(selectedVocab);
        auto word = dict.addWord(wordName.c_str(), ForthState::EXECUTABLE, ForthWordType::WORD, selectedVocab.c_str());

        // Check for successful addition and track it
        if (word == nullptr) {
            printf("Word %s was not added to vocabulary %s\n", wordName.c_str(), selectedVocab.c_str());
            FAIL() << "Word addition failed.";
        }

        // Keep track of the word added to this vocabulary
        vocabWords[selectedVocab].push_back(wordName);
    }

    // Step 4: Search for a random selection of 1,000 words
    std::vector<std::string> allRandomWords;
    for (const auto& [vocabName, words] : vocabWords) {
        allRandomWords.insert(allRandomWords.end(), words.begin(), words.end());
    }

    // Shuffle the words for randomness in testing
    std::shuffle(allRandomWords.begin(), allRandomWords.end(), generator);

    // Pick 1,000 random words
    size_t testSearchCount = 1000;
    if (allRandomWords.size() < testSearchCount) {
        testSearchCount = allRandomWords.size();
    }

    // Set the search order with all vocabularies
    dict.setSearchOrder(vocabNames);

    // Search for the words and ensure they're found
    for (size_t i = 0; i < testSearchCount; ++i) {
        std::string randomWord = allRandomWords[i];
        ForthDictionaryEntry* foundWord = dict.findWord(randomWord.c_str());
        EXPECT_NE(foundWord, nullptr) << "Word " << randomWord << " was not found.";
    }

    // Final step: Ensure that unique name pool has 0 remaining names
    EXPECT_EQ(uniqueNames.size(), 0) << "Not all unique names were consumed during the test.";

    SUCCEED(); // This will output performance statistics in Google Test.
}

// Main function for Google Test
int main(int argc, char **argv) {
    code_generator_initialize();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
