#include <gtest/gtest.h>
#include "JitContext.h"

// Test if JitContext initializes correctly
TEST(JitContextTest, Initialization) {
    printf("JitContextTest\n");
    EXPECT_NO_THROW(JitContext::instance().initialize());
}

// Test if AsmJit code holder is initialized correctly
TEST(JitContextTest, CodeHolderInitialization) {
    JitContext::instance().initialize();
    asmjit::CodeHolder& codeHolder = JitContext::instance().getCode();
    EXPECT_TRUE(codeHolder.isInitialized());
}



// Main function for Google Test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
