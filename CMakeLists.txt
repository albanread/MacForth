cmake_minimum_required(VERSION 3.16)

# Project name and version
project(ForthJIT VERSION 1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable warnings and optimizations
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Werror")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_compile_options(-Wno-gnu-anonymous-struct -Wno-nested-anon-types)

# Define source directories
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include" "/usr/local/include")
set(TEST_DIR "${CMAKE_SOURCE_DIR}/tests") # Optional: To make test file handling explicit.

# Collect all source files from src and include
file(GLOB_RECURSE SOURCES "${SRC_DIR}/*.cpp")

# Define main executable sources (include main.c)
set(MAIN_EXECUTABLE_SOURCES
        ${SOURCES}
        ${CMAKE_SOURCE_DIR}/main.cpp
        include/Quit.h
        src/Quit.cpp
        include/SymbolTable.h
        include/ControlFlow.h
        include/FlowLabels.h
        include/LexLet.h
        src/StringsStorage.cpp
        include/Optimizer.h
        src/Optimizer.cpp
        include/Settings.h
)

# Include directories
include_directories(${INCLUDE_DIR})

# **AsmJit Integration**
set(ASMJIT_LIB "/usr/local/lib/libasmjit.dylib")
set(ASMJIT_LIB_DIR "/usr/local/lib")
include_directories(${ASMJIT_DIR}/src)

# **RPATH Configuration**
set(CMAKE_INSTALL_RPATH "${ASMJIT_LIB_DIR}")
set(CMAKE_BUILD_RPATH "${ASMJIT_LIB_DIR}")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
list(APPEND CMAKE_SKIP_BUILD_RPATH FALSE)
list(APPEND CMAKE_SKIP_INSTALL_RPATH FALSE)

# **Main Executable**
add_executable(ForthJIT ${MAIN_EXECUTABLE_SOURCES})

set_target_properties(ForthJIT PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)
target_link_libraries(ForthJIT PRIVATE ${ASMJIT_LIB})
set_target_properties(ForthJIT PROPERTIES
        INSTALL_RPATH "${ASMJIT_LIB_DIR}"
        BUILD_WITH_INSTALL_RPATH TRUE
)

if(APPLE)
    message(STATUS "Building on macOS")
    target_link_libraries(ForthJIT PRIVATE pthread ${ASMJIT_LIB})
    target_compile_options(ForthJIT PRIVATE -g)
endif()

# **Google Test Integration**
enable_testing()
find_package(GTest REQUIRED)

if(NOT GTest_FOUND)
    set(GTEST_INCLUDE_DIR "/usr/local/include")
    set(GTEST_LIB_DIR "/usr/local/lib")
    include_directories(${GTEST_INCLUDE_DIR})
    link_directories(${GTEST_LIB_DIR})
endif()

include_directories(${GTEST_INCLUDE_DIRS})

# Shared RPATH for test executables
set(TEST_RPATH "${ASMJIT_LIB_DIR}")

# **Test - CodeGenerator**
set(TEST1_FILE "${TEST_DIR}/test_CodeGenerator.cpp")
add_executable(ForthJIT_test_CodeGenerator ${TEST1_FILE} ${SOURCES})
target_link_libraries(ForthJIT_test_CodeGenerator PRIVATE GTest::GTest GTest::Main pthread ${ASMJIT_LIB})
set_target_properties(ForthJIT_test_CodeGenerator PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
        INSTALL_RPATH "${TEST_RPATH}"
        BUILD_RPATH "${TEST_RPATH}"
        BUILD_WITH_INSTALL_RPATH TRUE
)

add_test(NAME RunTest_CodeGenerator COMMAND ForthJIT_test_CodeGenerator)
set_tests_properties(RunTest_CodeGenerator PROPERTIES RESOURCE_LOCK stack)

# **Test - JitContext**
set(TEST2_FILE "${TEST_DIR}/test_JitContext.cpp")
add_executable(ForthJIT_test_JitContext ${TEST2_FILE} ${SOURCES})
target_link_libraries(ForthJIT_test_JitContext PRIVATE GTest::GTest GTest::Main pthread ${ASMJIT_LIB})
set_target_properties(ForthJIT_test_JitContext PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
        INSTALL_RPATH "${TEST_RPATH}"
        BUILD_RPATH "${TEST_RPATH}"
        BUILD_WITH_INSTALL_RPATH TRUE
)
add_test(NAME RunTest_JitContext COMMAND ForthJIT_test_JitContext)

# **Test - ForthDictionary (New Test)**
set(TEST3_FILE "${TEST_DIR}/testForthDictionary.cpp")
add_executable(ForthJIT_test_ForthDictionary ${TEST3_FILE} ${SOURCES})
target_link_libraries(ForthJIT_test_ForthDictionary PRIVATE GTest::GTest GTest::Main pthread ${ASMJIT_LIB})
set_target_properties(ForthJIT_test_ForthDictionary PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
        INSTALL_RPATH "${TEST_RPATH}"
        BUILD_RPATH "${TEST_RPATH}"
        BUILD_WITH_INSTALL_RPATH TRUE
)
add_test(NAME RunTest_ForthDictionary COMMAND ForthJIT_test_ForthDictionary)

# **Test - Tokenizer (New Test)**
set(TEST4_FILE "${TEST_DIR}/test_Tokenizer.cpp")
add_executable(ForthJIT_test_Tokenizer ${TEST4_FILE} ${SOURCES})
target_link_libraries(ForthJIT_test_Tokenizer PRIVATE GTest::GTest GTest::Main pthread ${ASMJIT_LIB})
set_target_properties(ForthJIT_test_Tokenizer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
        INSTALL_RPATH "${TEST_RPATH}"
        BUILD_RPATH "${TEST_RPATH}"
        BUILD_WITH_INSTALL_RPATH TRUE
)
add_test(NAME RunTest_Tokenizer COMMAND ForthJIT_test_Tokenizer)


# Display build information
message(STATUS "Building ForthJIT with AsmJit and tests: test_CodeGenerator, test_JitContext, testForthDictionary")

