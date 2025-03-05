#include <gtest/gtest.h>
#include "CodeGenerator.h"
#include "JitContext.h"
#include "ForthDictionary.h"

// Forward declarations for cpush and cpop stack helpers
extern void cpush(int64_t value);
extern int64_t cpop();
uint64_t fetchR15();
uint64_t fetchR13();
uint64_t fetchR12();

// A simple C function to be called by JIT
extern "C" void test_function() {
    printf("Hello from C!\n");
}

TEST(StackOperations, TestDUP) {
    code_generator_initialize();
    // Arrange
    std::cout <<  "START  SP = " << fetchR15() << " TOS: " << fetchR13() << " TOS-1: " << fetchR12() << std::endl;
    cpush(42); // Push a single value onto the stack
    std::cout <<  "PUSH42 SP = " << fetchR15() << " TOS: " << fetchR13() << " TOS-1: " << fetchR12() << std::endl;
    // Act
    ForthDictionary::instance().execWord("DUP");
    std::cout <<  "DUP    SP = " << fetchR15() << " TOS: " << fetchR13() << " TOS-1: " << fetchR12() << std::endl;

    // Assert
    int64_t top = cpop();  // Get the top of the stack

    int64_t duplicate = cpop();  // Get the second value (duplicate)

    EXPECT_EQ(top, 42);  // Top should be the duplicate of the original value
    EXPECT_EQ(duplicate, 42);  // Second value should be the original value
}

TEST(StackOperations, TestSWAP) {
    code_generator_initialize();
    // Arrange
    std::cout <<  "START    SP = " << fetchR15() << " TOS: " << fetchR13() << " TOS-1: " << fetchR12() << std::endl;
    cpush(42);  // Push first value

    std::cout <<  "PUSH42   SP = " << fetchR15() << " TOS: " << fetchR13() << " TOS-1: " << fetchR12() << std::endl;
    cpush(17);  // Push second value

    std::cout <<  "PUSH 17  SP = " << fetchR15() << " TOS: " << fetchR13() << " TOS-1: " << fetchR12() << std::endl;
    // Act
    ForthDictionary::instance().execWord("SWAP");
    std::cout <<  "SWAP      SP = " << fetchR15() << " TOS: " << fetchR13() << " TOS-1: " << fetchR12() << std::endl;
    // Assert
    int64_t top = cpop();  // Top of the stack after SWAP
    int64_t bottom = cpop();  // Second value after SWAP
    EXPECT_EQ(top, 42);  // The first value should now be on top
    EXPECT_EQ(bottom, 17);  // The second value should now be on the bottom
}

TEST(StackOperations, TestROT) {
    code_generator_initialize();
    std::cout <<  "START    SP = " << fetchR15() << " TOS: " << fetchR13() << " TOS-1: " << fetchR12() << std::endl;

    cpush(1);  // x1
    cpush(2);  // x2
    cpush(3);  // x3
    std::cout <<  "PUSH 1,2,3 SP = " << fetchR15() << " TOS: " << fetchR13() << " TOS-1: " << fetchR12() << std::endl;

    // Act
    ForthDictionary::instance().execWord("ROT");
    std::cout <<  "ROT SP = " << fetchR15() << " TOS: " << fetchR13() << " TOS-1: " << fetchR12() << std::endl;

    // Assert
    int64_t top = cpop();  // x1 should be moved to the top
    int64_t mid = cpop();  // x3 should now be in the middle
    int64_t bottom = cpop();  // x2 should be on the bottom
    EXPECT_EQ(top, 1);
    EXPECT_EQ(mid, 3);
    EXPECT_EQ(bottom, 2);
}

TEST(StackOperations, TestOVER) {
    code_generator_initialize();
    std::cout <<  "START    SP = " << fetchR15() << " TOS: " << fetchR13() << " TOS-1: " << fetchR12() << std::endl;

    cpush(99);  // x1
    cpush(100); // x2
    std::cout <<  "99,100   SP = " << fetchR15() << " TOS: " << fetchR13() << " TOS-1: " << fetchR12() << std::endl;

    // Act
    ForthDictionary::instance().execWord("OVER");
    std::cout <<  "OVER  SP = " << fetchR15() << " TOS: " << fetchR13() << " TOS-1: " << fetchR12() << std::endl;

    // Assert
    int64_t top = cpop();      // x1 (copied value)
    int64_t middle = cpop();   // x2 (original)
    int64_t bottom = cpop();   // x1 (original)
    EXPECT_EQ(top, 99);        // x1 copied
    EXPECT_EQ(middle, 100);    // x2 remains unchanged
    EXPECT_EQ(bottom, 99);     // x1 remains unchanged
}

TEST(StackOperations, TestNIP) {
    code_generator_initialize();
    // Arrange
    cpush(101);  // x1
    cpush(202);  // x2

    // Act
    ForthDictionary::instance().execWord("NIP");

    // Assert
    int64_t top = cpop();  // Result after NIP
    EXPECT_EQ(top, 202);   // x2 should have removed x1 from the stack
}



TEST(StackOperations, Test2DUP) {
    code_generator_initialize();
    // Arrange
    cpush(11);  // x1
    cpush(22);  // x2

    // Act
    ForthDictionary::instance().execWord("2DUP");

    // Assert
    int64_t top = cpop();  // x1 copy
    int64_t mid = cpop();  // x2 copy
    int64_t bottom1 = cpop(); // x2 original
    int64_t bottom2 = cpop(); // x1 original
    EXPECT_EQ(top, 22);      // Copy of x1
    EXPECT_EQ(mid, 11);      // Copy of x2
    EXPECT_EQ(bottom1, 22);  // Original x2
    EXPECT_EQ(bottom2, 11);  // Original x1
}


TEST(StackOperations, TestAddition) {
    code_generator_initialize();

    // Arrange
    cpush(10);  // x1
    cpush(20);  // x2

    // Act
    ForthDictionary::instance().execWord("+");

    // Assert
    int64_t result = cpop();
    EXPECT_EQ(result, 30);  // 10 + 20 = 30
}

TEST(StackOperations, TestSubtraction) {
    code_generator_initialize();

    // Arrange
    cpush(20);  // x1
    cpush(50);  // x2

    // Act
    ForthDictionary::instance().execWord("-");

    // Assert
    int64_t result = cpop();
    EXPECT_EQ(result, -30);  // 20 - 50 = -30
}

TEST(StackOperations, TestMultiplication) {
    code_generator_initialize();

    // Arrange
    cpush(6);   // x1
    cpush(7);   // x2

    // Act
    ForthDictionary::instance().execWord("*");

    // Assert
    int64_t result = cpop();
    EXPECT_EQ(result, 42);  // 6 * 7 = 42
}

TEST(StackOperations, TestDivision) {
    code_generator_initialize();

    // Arrange
    cpush(10);   // x1 (divisor)
    cpush(5);  // x2 (dividend)

    // Act
    ForthDictionary::instance().execWord("/");

    // Assert
    int64_t result = cpop();
    EXPECT_EQ(result, 2);  // 10 / 5 = 2
}

TEST(StackOperations, TestModulus) {
    code_generator_initialize();

    // Arrange
    cpush(10);   // x1 (modulus)
    cpush(3);  // x2 (dividend)

    // Act
    ForthDictionary::instance().execWord("MOD");

    // Assert
    int64_t result = cpop();
    EXPECT_EQ(result, 1);  // 10 % 3 = 1
}

TEST(StackOperations, TestMultiplyThenDivide) {
    code_generator_initialize();

    // Arrange
    cpush(6);   // x1 (divisor)
    cpush(4);   // x2 (multiplier)
    cpush(3);   // x3 (value to be multiplied)

    // Act
    ForthDictionary::instance().execWord("*/");

    // Assert
    int64_t result = cpop();
    EXPECT_EQ(result, 8);  // (6 * 4) / 3 = 8
}
//
TEST(StackOperations, TestMultiplyDivideModulus) {
    code_generator_initialize();

    // Arrange
    cpush(10);    // x1 (divisor)
    cpush(5);    // x2 (multiplier)
    cpush(3);   // x3 (value to be multiplied)

    // Act
    ForthDictionary::instance().execWord("*/MOD");

    // Assert
    int64_t quotient = cpop();       // Division result
    int64_t remainder = cpop();      // Modulus result
    EXPECT_EQ(quotient, 16);
    EXPECT_EQ(remainder, 2);
}


TEST(FloatingPointOperations, TestFAddition) {
    code_generator_initialize();

    // Arrange
    cfpush(10.5);  // Push first float
    cfpush(20.25); // Push second float

    // Act
    ForthDictionary::instance().execWord("F+");

    // Assert
    double result = cfpop();
    EXPECT_DOUBLE_EQ(result, 30.75);  // 10.5 + 20.25 = 30.75
}

TEST(FloatingPointOperations, TestFSubtraction) {
    code_generator_initialize();

    // Arrange
    cfpush(50.75);  // Push first float
    cfpush(20.25);  // Push second float

    // Act
    ForthDictionary::instance().execWord("F-");

    // Assert
    double result = cfpop();
    EXPECT_DOUBLE_EQ(result, 30.5);  // 50.75 - 20.25 = 30.5
}

TEST(FloatingPointOperations, TestFMultiplication) {
    code_generator_initialize();

    // Arrange
    cfpush(3.5);  // Push first float
    cfpush(2.0);  // Push second float

    // Act
    ForthDictionary::instance().execWord("F*");

    // Assert
    double result = cfpop();
    EXPECT_DOUBLE_EQ(result, 7.0);  // 3.5 * 2.0 = 7.0
}

TEST(FloatingPointOperations, TestFDivision) {
    code_generator_initialize();

    // Arrange
    cfpush(22.0);  // Push first float
    cfpush(7.0);   // Push second float

    // Act
    ForthDictionary::instance().execWord("F/");

    // Assert
    double result = cfpop();
    EXPECT_DOUBLE_EQ(result, 22.0 / 7.0);  // Division of two floats
}

TEST(FloatingPointOperations, TestFLessThan) {
    code_generator_initialize();

    // Arrange
    cfpush(10.0);  // Push first float
    cfpush(20.0);  // Push second float

    // Act
    ForthDictionary::instance().execWord("F<");

    // Assert
    int64_t result = cpop();
    EXPECT_EQ(result, -1);  // 10.0 < 20.0 is true (-1)
}

TEST(FloatingPointOperations, TestFGreaterThan) {
    code_generator_initialize();

    // Arrange
    cfpush(20.0);  // Push first float
    cfpush(10.0);  // Push second float

    // Act
    ForthDictionary::instance().execWord("F>");

    // Assert
    int64_t result = cpop();
    EXPECT_EQ(result, -1);  // 20.0 > 10.0 is true (1)
}

TEST(FloatingPointOperations, TestFEqual) {
    code_generator_initialize();

    // Arrange
    cfpush(15.0);  // Push first float
    cfpush(15.0);  // Push second float

    // Act
    ForthDictionary::instance().execWord("F=");

    // Assert
    int64_t result = cpop();
    EXPECT_EQ(result, -1);  // 15.0 == 15.0 is true
}


TEST(FloatingPointOperations, TestSquareRoot) {
    code_generator_initialize();

    // Arrange
    cfpush(16.0);  // Push a float

    // Act
    ForthDictionary::instance().execWord("FSQRT");

    // Assert
    double result = cfpop();
    EXPECT_DOUBLE_EQ(result, 4.0);  // SQRT(16.0) = 4.0
}





// Main function for Google Test
int main(int argc, char **argv) {

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
