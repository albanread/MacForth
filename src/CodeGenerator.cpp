// code generator
// in theory this is the part that needs to change for different systems.

#include <CodeGenerator.h>
#include "JitContext.h"
#include <cstdint>
#include <ForthDictionary.h>
#include "LabelManager.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <StringsStorage.h>
#include <unistd.h>
#include <gtest/gtest-printers.h>
#include "SignalHandler.h"
#include "Settings.h"




void *code_generator_heap_start = nullptr;

LabelManager labels;

// Global pointers to track the stack memory
uintptr_t stack_base = 0; // Start of the stack memory
uintptr_t stack_top = 0; // The "top" pointer (where R15 begins descending)


// Global pointers to track the return stack memory
uintptr_t return_stack_base = 0; // Start of the return stack memory
uintptr_t return_stack_top = 0; // The "top" pointer (where R14 begins descending)

// JIT-d function pointer type
typedef void (*JitFunction)(ForthFunction);

uint64_t fetchR15();


// Function to set up the stack pointer and clear registers using inline assembly
extern "C" void stack_setup_asm(long stackTop) {
    asm volatile(
        "mov %[stack_pointer], %%r15\n\t" // Set R15 to the stackTop address
        "xor %%r12, %%r12\n\t" // Clear R12
        "xor %%r13, %%r13\n\t" // Clear R13
        :
        : [stack_pointer] "r"(stackTop) // Input: stackTop passed in via registers

    );
}

// this function relies on a certain amount of luck
// e.g. R15 needs not to be changed by this function.
// Stack setup in C using inline assembly
void *stack_setup() {
    constexpr size_t STACK_SIZE = 4 * 1024 * 1024; // 4MB
    constexpr size_t UNDERFLOW_GAP = 64;

    // Step 1: Allocate memory for the stack

    // ReSharper disable once CppDFAMemoryLeak
    void *stackBase = std::malloc(STACK_SIZE);
    if (!stackBase) {
        std::cerr << "Stack allocation failed!\n";
        return nullptr;
    }

    // Step 2: Initialize the stack memory to zero
    std::memset(stackBase, 0, STACK_SIZE);

    // Step 3: Compute the stack top
    void *stackTop = static_cast<char *>(stackBase) + STACK_SIZE - UNDERFLOW_GAP;

    // LUCK needed here
    stack_setup_asm(reinterpret_cast<long>(stackTop));

    stack_base = reinterpret_cast<uintptr_t>(stackBase);
    stack_top = reinterpret_cast<uintptr_t>(stackTop);
    //long r15 = fetchR15();
    //printf("r15=%ld\n", r15);
    return stackBase; // Return the base of the allocated stack for any further usage
}


extern "C" void return_stack_setup_asm(long stackTop) {
    asm volatile(
        "mov %[stack_pointer], %%r14\n\t" // Set R14 to the stackTop address
        :
        : [stack_pointer] "r"(stackTop) // Input: stackTop passed in via registers
    );
}

// Function to set up the return stack in C
void *return_stack_setup() {
    constexpr size_t STACK_SIZE = 1 * 1024 * 1024; // 4MB for the return stack
    constexpr size_t UNDERFLOW_GAP = 64;

    // Step 1: Allocate memory for the return stack
    // ReSharper disable once CppDFAMemoryLeak
    void *return_stackBase = std::malloc(STACK_SIZE);
    if (!return_stackBase) {
        std::cerr << "Return stack allocation failed!\n";
        return nullptr;
    }

    // Step 2: Initialize the return stack memory to zero
    std::memset(return_stackBase, 0, STACK_SIZE);

    // Step 3: Compute the return stack top
    void *return_stackTop = static_cast<char *>(return_stackBase) + STACK_SIZE - UNDERFLOW_GAP;


    return_stack_base = reinterpret_cast<uintptr_t>(return_stackBase);
    return_stack_top = reinterpret_cast<uintptr_t>(return_stackTop);
    // Step 4: Set up the return stack pointer using inline assembly
    return_stack_setup_asm(reinterpret_cast<long>(return_stackTop));

    return return_stackBase; // Return the base of the allocated return stack
}

void check_logging() {
    if (jitLogging == true) {
        JitContext::instance().enableLogging(true, true);
    } else {
        JitContext::instance().disableLogging();
    }
}

bool initialize_assembler(asmjit::x86::Assembler *&assembler) {
    assembler = &JitContext::instance().getAssembler();
    if (!assembler) {
        SignalHandler::instance().raise(10);
        return true;
    }
    check_logging();
    return false;
}


// set the code for an entry to stack its own address.
void set_stack_self(const ForthDictionaryEntry *e) {
    code_generator_startFunction(e->getWordName());

    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    // assembler->commentf("Address of dictionary entry %p", e->getAddress());
    assembler->mov(asmjit::x86::rax, asmjit::imm(e->getAddress()));
    // Copy the value from rax into rbp
    assembler->mov(asmjit::x86::rbp, asmjit::x86::rax);
    assembler->comment("; ----- stack word address");
    pushDS(asmjit::x86::rbp);
    labels.createLabel(*assembler, "exit_function");
    labels.bindLabel(*assembler, "exit_function");
    compile_return();
    const auto fn = code_generator_finalizeFunction(e->getWordName());
    e->executable = fn;
}


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

void track_heap() {
    code_generator_heap_start = sbrk(0);
}

bool isHeapPointer(void *ptr, void *heap_start) {
    return (ptr >= heap_start); // Valid if the pointer is on the heap
}

void validatePointer(void *ptr, void *heap_start) {
    if (!isHeapPointer(ptr, heap_start)) {
        std::cerr << "Pointer " << ptr << " is not in the heap!" << std::endl;
        abort();
    }
}

void printHeapGrowth(void *heap_start) {
    void *current_break = sbrk(0);
    size_t heap_growth = static_cast<char *>(current_break) - static_cast<char *>(heap_start);
    std::cout << "Heap growth: " << heap_growth << " bytes" << std::endl;
}

#pragma clang diagnostic pop


void code_generator_initialize() {
    track_heap();
    optimizer = true;

    JitContext::instance().initialize();
    JitContext::instance().disableLogging();

    asmjit::x86::Assembler *assembler = &JitContext::instance().getAssembler();
    if (!assembler) {
        SignalHandler::instance().raise(10);
        return;
    }


    // ReSharper disable once CppDFAMemoryLeak
    stack_setup();
    // ReSharper disable once CppDFAMemoryLeak
    return_stack_setup();

    auto &dict = ForthDictionary::instance();

    std::cout << "FORTH creating dictionary" << std::endl;
    const auto e = dict.createVocabulary("FORTH");
    set_stack_self(e);

    const auto e1 = dict.createVocabulary("FRAGMENTS");
    set_stack_self(e1);

    dict.setVocabulary("FORTH");
    dict.setSearchOrder({"FORTH", "FRAGMENTS"});

    code_generator_add_stack_words();
    code_generator_add_operator_words();
    code_generator_add_immediate_words();
    code_generator_add_io_words();
    code_generator_add_control_flow_words();
    code_generator_add_vocab_words();
    code_generator_add_float_words();

    std::cout << "FORTH dictionary created." << std::endl;
}


// helpers


void compile_pushLiteral(const int64_t literal) {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    assembler->comment("; -- LITERAL (make space)");
    assembler->sub(asmjit::x86::r15, 8);
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r15), asmjit::x86::r12);
    assembler->mov(asmjit::x86::r12, asmjit::x86::r13);
    // 4. Load the given literal into TOS (R13)

    assembler->mov(asmjit::x86::r13, asmjit::imm(literal));
    assembler->commentf("; -- TOS is %lld \n", literal);
}


// * stack helpers
// * ------------------
// * 1. **pushDS(Gp reg)**:
// *    Pushes the value from the specified register onto the data stack.
// *    Updates R12 and R13 to maintain the caching protocol.
// *
// * 2. **popDS(Gp reg)**:
// *    Pops the top value from the stack into the specified register.
// *    Updates R12 and R13 to reflect the new TOS and TOS-1 values.
// *
// * 3. **loadDS(void *dataAddress)**:
// *    Loads the value at the specified memory address and pushes it onto the stack.
// *
// * 4. **loadFromDS()**:
// *    Pops an address from the stack, dereferences it, and pushes the resultant value back.
// *
// * 5. **storeDS(void *dataAddress)**:
// *    Pops the top value from the stack and stores it in the specified memory address.
// *
// * 6. **storeFromDS()**:
// *    Pops both a destination address and a value from the stack, and stores the value at the address.
// *
//


[[maybe_unused]] static void pushDS(const asmjit::x86::Gp &reg) {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    assembler->comment("; ----- pushDS");
    assembler->comment("; Save TOS (R13) to the data stack and update R12/R13");

    // Save the current TOS (R13) to the stack
    assembler->mov(asmjit::x86::qword_ptr(asmjit::x86::r15), asmjit::x86::r13);
    assembler->sub(asmjit::x86::r15, 8); // Decrement DSP (R15)

    // Update TOS (R13 -> R12, new value becomes TOS)
    assembler->mov(asmjit::x86::r12, asmjit::x86::r13); // R12 = Old TOS
    assembler->mov(asmjit::x86::r13, reg); // New TOS cached in R13
}

[[maybe_unused]] static void popDS(const asmjit::x86::Gp &reg) {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    assembler->comment("; -- POP DS to register");

    // Move the current TOS (R13) into the specified register
    assembler->mov(reg, asmjit::x86::r13);
    assembler->comment("; DROP TOS ");
    assembler->mov(asmjit::x86::r13, asmjit::x86::r12); // Move TOS-1 into TOS
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Move TOS-2 into TOS-1
    assembler->add(asmjit::x86::r15, 8); // Adjust stack pointer
}

[[maybe_unused]] static void loadDS(void *dataAddress) {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    assembler->comment("; ----- loadDS");
    assembler->comment("; Dereference a memory address and push the value onto the stack");

    // Load the address into RAX
    assembler->mov(asmjit::x86::rax, dataAddress);

    // Dereference the address to get the value
    assembler->mov(asmjit::x86::rax, asmjit::x86::ptr(asmjit::x86::rax));

    // Push the value onto the data stack using the protocol
    pushDS(asmjit::x86::rax);
}

[[maybe_unused]] static void loadFromDS() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    assembler->comment("; -- load from DS");
    assembler->comment("; Pop address, dereference, push the value");

    // Pop the address into RAX
    popDS(asmjit::x86::rax);

    // Dereference the address to get the value
    assembler->mov(asmjit::x86::rax, asmjit::x86::ptr(asmjit::x86::rax));

    // Push the value onto the data stack
    pushDS(asmjit::x86::rax);
}

[[maybe_unused]] static void storeDS(void *dataAddress) {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    assembler->comment("; ----- storeDS");
    assembler->comment("; Pop and store at address");

    // Pop the value into RAX
    popDS(asmjit::x86::rax);

    // Load the specified address into RCX
    assembler->mov(asmjit::x86::rcx, dataAddress);

    // Store the value at the address
    assembler->mov(asmjit::x86::qword_ptr(asmjit::x86::rcx), asmjit::x86::rax);
}

[[maybe_unused]] static void storeFromDS() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    assembler->comment("; -- store from DS");
    assembler->comment("; Pop address and value, store  value in address");

    // Pop the address into RCX
    popDS(asmjit::x86::rcx);

    // Pop the value into RAX
    popDS(asmjit::x86::rax);

    // Store the value from RAX at the address in RCX
    assembler->mov(asmjit::x86::qword_ptr(asmjit::x86::rcx), asmjit::x86::rax);
}

[[maybe_unused]] static void fetchFromDS() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    assembler->comment("; ----- fetch from DS (@)");
    assembler->comment("; Pop address, fetch value, and push");

    // Pop the address from the stack into RCX
    popDS(asmjit::x86::rcx);

    // Fetch the value at the address (RCX) into RAX
    assembler->mov(asmjit::x86::rax, asmjit::x86::qword_ptr(asmjit::x86::rcx));

    // Push the fetched value (RAX) back onto the stack
    pushDS(asmjit::x86::rax);
}


[[maybe_unused]] static void pushRS(const asmjit::x86::Gp &reg) {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    //
    assembler->comment("; -- pushRS from register");
    assembler->comment("; save value to the return stack (r14)");

    assembler->sub(asmjit::x86::r14, 8);
    assembler->mov(asmjit::x86::qword_ptr(asmjit::x86::r14), reg);
}

[[maybe_unused]] static void popRS(const asmjit::x86::Gp &reg) {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    //
    assembler->comment("; -- popRS to register");
    assembler->comment("; fetch value from the return stack (r14)");

    assembler->mov(reg, asmjit::x86::qword_ptr(asmjit::x86::r14));
    assembler->add(asmjit::x86::r14, 8);
}


// genFetch - fetch the contents of the address
[[maybe_unused]] static void genFetch(uint64_t address) {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    asmjit::x86::Gp addr = asmjit::x86::rax; // General purpose register for address
    asmjit::x86::Gp value = asmjit::x86::rdi; // General purpose register for the value
    assembler->mov(addr, address); // Move the address into the register.
    assembler->mov(value, asmjit::x86::ptr(addr));
    pushDS(value); // Push the value onto the stack.
}


void code_generator_align(asmjit::x86::Assembler *assembler) {
    assembler->comment("; ----- align on 16 byte boundary");
    assembler->align(asmjit::AlignMode::kCode, 16);
}


// call at function start
void code_generator_startFunction(const std::string &name) {
    JitContext::instance().initialize();
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    assembler->align(asmjit::AlignMode::kCode, 16);
    assembler->commentf("; -- enter function: %s ", name.c_str());
    labels.clearLabels();
    labels.createLabel(*assembler, "enter_function");
    labels.bindLabel(*assembler, "enter_function");
    labels.createLabel(*assembler, "exit_label");
    const ForthDictionary &dict = ForthDictionary::instance();
    auto entry = dict.getLatestWordAdded();
    if (jitLogging && entry != nullptr) {
        entry->display();
        entry->displayOffsets();
    }


    // assembler->commentf("Address of dictionary entry %p", entry->getAddress());

    FunctionEntryExitLabel funcLabels;
    funcLabels.entryLabel = assembler->newLabel();
    funcLabels.exitLabel = assembler->newLabel();
    assembler->bind(funcLabels.entryLabel);


    // Save on loopStack
    const LoopLabel loopLabel{LoopType::FUNCTION_ENTRY_EXIT, funcLabels};
    loopStack.push(loopLabel);

    //
    assembler->comment("; ----- RBP is set to dictionary entry");
    assembler->mov(asmjit::x86::rax, asmjit::imm(entry->getAddress()));
    // Copy the value from rax into rbp
    assembler->mov(asmjit::x86::rbp, asmjit::x86::rax);
}


void code_generator_emitMoveImmediate(int64_t value) {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    assembler->mov(asmjit::x86::rax, value);
}

void compile_return() {
    asmjit::x86::Assembler *assembler = &JitContext::instance().getAssembler();
    if (!assembler) {
        SignalHandler::instance().raise(10);
        return;
    }
    labels.bindLabel(*assembler, "exit_label");
    loopStack.pop();
    assembler->ret();
}

// call at end to get point to function generated
ForthFunction code_generator_finalizeFunction(const std::string &name) {
    std::string funcName = "; end of -- " + name + " --";
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    assembler->comment(funcName.c_str());


    return JitContext::instance().finalize();
}

void code_generator_reset() {
    return JitContext::instance().initialize();
}

void compile_call_C(void (*func)()) {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    assembler->comment("; --- call c code");
    assembler->push(asmjit::x86::rdi);
    assembler->call(func);
    assembler->pop(asmjit::x86::rdi);
    assembler->comment("; --- end call c ");
}

void compile_call_forth(void (*func)(), const std::string &forth_word) {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    // keep stack 16 byte aligned.
    assembler->commentf("; --- call %s", forth_word.c_str());
    assembler->sub(asmjit::x86::rsp, 8);
    assembler->call(func);
    assembler->add(asmjit::x86::rsp, 8);
}


void compile_call_C_char(void (*func)(char *)) {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    assembler->push(asmjit::x86::rdi);
    assembler->mov(asmjit::x86::rdi, asmjit::x86::r13);
    assembler->call(func);
    assembler->pop(asmjit::x86::rdi);
}


// Stack words
static void compile_DROP() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    // DROP ( x -- )
    assembler->comment("; -- DROP ");
    assembler->mov(asmjit::x86::r13, asmjit::x86::r12); // Move TOS-1 into TOS
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Move TOS-2 into TOS-1
    assembler->add(asmjit::x86::r15, 8); // Adjust stack pointer
}

static void compile_PICK() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    // PICK ( xn ... x1 n -- xn ... x1 xn ) Copy nth item to TOS
    assembler->comment("; -- PICK ");
    assembler->mov(asmjit::x86::rax, asmjit::x86::r13); // Get n (index)
    assembler->shl(asmjit::x86::rax, 3); // Convert to byte offset
    assembler->mov(asmjit::x86::r13, asmjit::x86::ptr(asmjit::x86::r15, asmjit::x86::rax)); // Load nth element into TOS
}


// stack words
// partial code generators

static void compile_ROT() {
    // ROT ( x1 x2 x3 -- x2 x3 x1 )
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    assembler->comment("; --ROT ");

    // Save TOS (x1, R13) into a temporary register (RAX)
    assembler->mov(asmjit::x86::rax, asmjit::x86::r13);

    // Move TOS-2 (x3, [R15]) into TOS (R13)
    assembler->mov(asmjit::x86::r13, asmjit::x86::ptr(asmjit::x86::r15));

    // Move TOS-1 (x2, R12) into TOS-2 ([R15])
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r15), asmjit::x86::r12);

    // Move the original TOS (x1, saved in RAX) into TOS-1 (R12)
    assembler->mov(asmjit::x86::r12, asmjit::x86::rax);

    assembler->comment("; End ROT ");
}

static void compile_MROT() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    // -ROT ( x1 x2 x3 -- x3 x1 x2 )
    assembler->comment("; ---ROT ");
    assembler->mov(asmjit::x86::rax, asmjit::x86::r13); // Save TOS (x3) in rax
    assembler->mov(asmjit::x86::r13, asmjit::x86::ptr(asmjit::x86::r15)); // Move x2 to TOS
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r15), asmjit::x86::r12); // Move x1 to stack (TOS-2)
    assembler->mov(asmjit::x86::r12, asmjit::x86::rax); // Restore x3 as TOS-1

    assembler->comment("; End -ROT ");
}

static void compile_SWAP() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    // SWAP ( x1 x2 -- x2 x1 )
    assembler->comment("; --SWAP ");
    assembler->xchg(asmjit::x86::r13, asmjit::x86::r12); // Swap TOS and TOS-1
    assembler->comment("; End SWAP ");
}

static void compile_OVER() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    using namespace asmjit;
    // OVER ( x1 x2 -- x1 x2 x1 )
    assembler->comment("; --OVER ");
    assembler->sub(x86::r15, imm(8)); // Decrement stack pointer to create space for new value
    assembler->mov(ptr(x86::r15), x86::r12); // Store TOS-1 (r12) in memory at [r15]
    assembler->mov(x86::r12, x86::r13); // Update TOS-1 (r12) to TOS (r13)
    assembler->mov(x86::r13, ptr(x86::r15)); // Reload the duplicated value from memory into TOS (r13)

}

static void compile_NIP() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    // NIP ( x1 x2 -- x2 )
    assembler->comment("; -- NIP ");
    assembler->add(asmjit::x86::r15, 8); // Discard TOS-1 (move stack pointer up)
    assembler->mov(asmjit::x86::r12, asmjit::x86::r13); // Move TOS into TOS-1


}

// R15 full descending stack R13=TOS, R12=TOS-1 [R15]=TOS-2
static void compile_TUCK() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);


    assembler->comment("; -- TUCK ");

    // Step 1: Allocate space for new TOS-2 (push duplicate of TOS)
    assembler->sub(asmjit::x86::r15, 8); // Full descending stack: subtract 8

    // Step 2: Store duplicate of TOS (x2 in R13) into the new slot.
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r15), asmjit::x86::r13);


}

static void compile_2DUP() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    // 2DUP ( x1 x2 -- x1 x2 x1 x2 )
    assembler->comment("; -- 2DUP ");

    assembler->sub(asmjit::x86::r15, 16); // Allocate space for two 64-bit values
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r15, 8), asmjit::x86::r12); // Store x1 (TOS-1) in stack
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r15), asmjit::x86::r13); // Store x2 (TOS) in stack
}

static void compile_2DROP() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    // 2DROP ( x1 x2 -- )
    assembler->comment("; -- 2DROP ");
    assembler->add(asmjit::x86::r15, 16); // Deallocate two 64-bit values from stack
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15, -16)); // Load new TOS
    assembler->mov(asmjit::x86::r13, asmjit::x86::ptr(asmjit::x86::r15, -8)); // Load new TOS-1
}


static void compile_DUP() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    // DUP ( -- x x )
    assembler->comment("; -- DUP ");
    assembler->sub(asmjit::x86::r15, 8); // Allocate space on the stack
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r15), asmjit::x86::r12); // Store TOS at new stack location
    assembler->mov(asmjit::x86::r12, asmjit::x86::r13); // Copy TOS to TOS-1
}


static void compiler_2OVER() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    // 2OVER ( x1 x2 x3 x4 -- x1 x2 x3 x4 x1 x2 )
    assembler->comment("; -- 2OVER ");
    assembler->sub(asmjit::x86::r15, 16); // Allocate space for two values
    assembler->mov(asmjit::x86::rax, asmjit::x86::ptr(asmjit::x86::r15, 24)); // Load x1 into rax
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r15, 8), asmjit::x86::rax); // Store x1 in new stack slot
    assembler->mov(asmjit::x86::rax, asmjit::x86::ptr(asmjit::x86::r15, 16)); // Load x2 into rax
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r15), asmjit::x86::rax); // Store x2 in new stack slot
}


static void compile_ROLL() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    // ROLL ( xn ... x1 n -- x(n-1) ... x1 xn ) Move nth element to TOS
    assembler->comment("; -- ROLL ");

    labels.createLabel(*assembler, "loop_roll");
    labels.createLabel(*assembler, "end_roll");

    // Generate the main assembly for ROLL
    assembler->mov(asmjit::x86::rax, asmjit::x86::r13); // Get n (index)
    assembler->shl(asmjit::x86::rax, 3); // Convert to byte offset
    assembler->lea(asmjit::x86::rdx, asmjit::x86::ptr(asmjit::x86::r15, asmjit::x86::rax));
    // Address of nth element
    assembler->mov(asmjit::x86::rcx, asmjit::x86::ptr(asmjit::x86::rdx)); // Load nth element into rcx

    // Loop to shift elements down
    labels.bindLabel(*assembler, "loop_roll");
    assembler->cmp(asmjit::x86::rdx, asmjit::x86::r15); // Check if we reached the top of the stack
    labels.jle(*assembler, "end_roll"); // If true, jump to end of the loop

    assembler->mov(asmjit::x86::rbx, asmjit::x86::ptr(asmjit::x86::rdx, -8)); // Shift elements down
    assembler->mov(asmjit::x86::ptr(asmjit::x86::rdx), asmjit::x86::rbx);
    // Store shifted value in the current position
    assembler->sub(asmjit::x86::rdx, 8); // Move to the next position down
    assembler->jmp(labels.getLabel("loop_roll")); // Jump back to the start of the loop

    // End of loop
    labels.bindLabel(*assembler, "end_roll");
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r15), asmjit::x86::rcx);
    // Move the saved nth element to the top of the stack
}

static void compile_SP_STORE() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    // SP! ( addr -- ) Set the stack pointer
    assembler->comment("; -- SP! ");
    assembler->mov(asmjit::x86::r15, asmjit::x86::r13); // Update R15 with new stack pointer
    assembler->mov(asmjit::x86::r13, asmjit::x86::ptr(asmjit::x86::r15)); // Load new TOS from updated stack
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15, 8)); // Load new TOS-1 from stack
}


static void compile_AT() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    // SP@ ( -- addr ) Get the current stack pointer
    assembler->comment("; --SP@ ");
    assembler->sub(asmjit::x86::r15, 8); // Allocate space on stack
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r15), asmjit::x86::r13); // Push current TOS to stack
    assembler->mov(asmjit::x86::r13, asmjit::x86::r15); // Load stack pointer into TOS
    assembler->comment("; End SP@ ");
}

// these functions are used from C code
void cpush(int64_t value) {
    __asm__ __volatile__ (

        "subq $8, %%r15 \n" // Allocate 8 bytes on the stack (R15 points downward)
        "movq %%r12, (%%r15) \n" // Push current TOS (R13) onto the stack memory
        "movq %%r13, %%r12 \n" // Push current TOS (R13) onto the stack memory
        "movq %0, %%r13 \n" // Load the new value into TOS (R13)

        :
        : "r"(value) // Input: Pass 'value' into the assembly
        : "memory" // Clobbers: Indicate which registers/memory are modified
    );
}


void cfpush(double value) {
    __asm__ __volatile__ (

        "subq $8, %%r15 \n" // Allocate 8 bytes on the stack (R15 points downward)
        "movq %%r12, (%%r15) \n" // Push current TOS (R13) onto the stack memory
        "movq %%r13, %%r12 \n" // Push current TOS (R13) onto the stack memory
        "movq %0, %%r13 \n" // Load the new value into TOS (R13)

        :
        : "r"(value) // Input: Pass 'value' into the assembly
        : "memory" // Clobbers: Indicate which registers/memory are modified
    );
}

int64_t cpop() {
    int64_t result;
    __asm__ __volatile__ (
        "movq %%r13, %0 \n"
        "movq %%r12, %%r13 \n"
        "movq (%%r15), %%r12 \n"
        "addq $8, %%r15 \n"
        : "=r"(result)
        :
        : "memory" // Clobbers: Indicate which registers/memory are modified
    );
    return result;
}

double cfpop() {
    double result;
    __asm__ __volatile__ (
        "movq %%r13, %0 \n"
        "movq %%r12, %%r13 \n"
        "movq (%%r15), %%r12 \n"
        "addq $8, %%r15 \n"
        : "=r"(result)
        :
        : "memory" // Clobbers: Indicate which registers/memory are modified
    );
    return result;
}


uint64_t fetchR15() {
    uint64_t result;
    __asm__ __volatile__ (
        "movq %%r15, %0 \n" // Move the current value of R15 into the result variable
        : "=r"(result) // Output: Store R15 in the result
        :
        : // No clobbers as we're only reading R15
    );
    return result;
}

uint64_t fetchR14() {
    uint64_t result;
    __asm__ __volatile__ (
        "movq %%r14, %0 \n" // Move the current value of R15 into the result variable
        : "=r"(result) // Output: Store R15 in the result
        :
        : // No clobbers as we're only reading R15
    );
    return result;
}

uint64_t fetchR13() {
    uint64_t result;
    __asm__ __volatile__ (
        "movq %%r13, %0 \n" // Move the current value of R13 (TOS) into the result variable
        : "=r"(result) // Output: Store R13 in the result
        :
        : // No clobbers as we're only reading R13
    );
    return result;
}


uint64_t fetchR12() {
    uint64_t result;
    __asm__ __volatile__ (
        "movq %%r12, %0 \n" // Move the current value of R12 (TOS-1) into the result variable
        : "=r"(result) // Output: Store R12 in the result
        :
        : // No clobbers as we're only reading R12
    );
    return result;
}

uint64_t fetch3rd() {
    uint64_t result;
    __asm__ __volatile__ (
        "movq (%%r15), %0 \n" // Move the current value of R12 (TOS-1) into the result variable
        : "=r"(result) // Output: Store R12 in the result
        :
        : // No clobbers as we're only reading R12
    );
    return result;
}

uint64_t fetch4th() {
    uint64_t result;
    __asm__ __volatile__ (
        "movq 8(%%r15), %0 \n" // Move the current value of R12 (TOS-1) into the result variable
        : "=r"(result) // Output: Store R12 in the result
        :
        : // No clobbers as we're only reading R12
    );
    return result;
}


//


// Return stack at R14
//

// 1. **`>R ( x -- ) ( R: -- x )`:** Moves the `TOS` (stored in `r13`) to the return stack (`r14`), adjusts the return stack pointer, and updates the data stack values.
// 2. **`R> ( -- x ) ( R: x -- )`:** Retrieves the top of the return stack, moves it to the data stack, and adjusts the stack pointers accordingly.
// 3. **`R@ ( -- x ) ( R: x -- x )`:** Copies the top of the return stack to `TOS` without modifying the return stack pointer (`r14`).
// 4. **`RP@ ( -- addr )`:** Pushes the current return stack pointer (`r14`) value onto the `TOS`.
// 5. **`RP! ( addr -- )`:** Sets the return stack pointer (`r14`) to a new address, updating `TOS` and adjusting the data stack accordingly.

static void Compile_toR() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    // >R ( x -- ) ( R: -- x )
    assembler->comment("; -->R ");
    assembler->sub(asmjit::x86::r14, 8); // Allocate space on return stack
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r14), asmjit::x86::r13); // Store TOS in return stack
    assembler->mov(asmjit::x86::r13, asmjit::x86::r12); // Move TOS-1 to TOS
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Load TOS-2 into TOS-1
    assembler->add(asmjit::x86::r15, 8); // Adjust data stack pointer
    assembler->comment("; End >R ");
}

// useful for DO .. LOOP
static void Compile_2toR() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    // 2>R ( x1 x2 -- ) ( R: -- x1 x2 )
    assembler->comment("; --2>R ");

    assembler->sub(asmjit::x86::r14, 8);
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r14), asmjit::x86::r12);

    assembler->sub(asmjit::x86::r14, 8);
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r14), asmjit::x86::r13);

    // Update R13 (TOS) with the value of TOS-2 (data stack at [R15])
    compile_2DROP();
}


static void Compile_fromR() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    // R> ( -- x ) ( R: x -- )
    assembler->comment("; --R> ");
    assembler->sub(asmjit::x86::r15, 8); // Allocate space on data stack
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r15), asmjit::x86::r12); // Store TOS-1 in stack
    assembler->mov(asmjit::x86::r12, asmjit::x86::r13); // Move TOS to TOS-1
    assembler->mov(asmjit::x86::r13, asmjit::x86::ptr(asmjit::x86::r14)); // Load from return stack into TOS
    assembler->add(asmjit::x86::r14, 8); // Adjust return stack pointer
    assembler->comment("; End R> ");
}

static void Compile_2fromR() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    // 2R> ( -- x1 x2 ) ( R: x1 x2 -- )
    assembler->comment("; -- 2R> ");
    assembler->comment("; -- make space ");
    compile_2DUP(); // move R13. R12 to datstack

    // over write R13, R12 from return stack
    assembler->comment("; -- R13, R12 from return stack ");
    assembler->mov(asmjit::x86::r13, asmjit::x86::ptr(asmjit::x86::r14));
    assembler->add(asmjit::x86::r14, 8); // Adjust return stack pointer (pop x1)
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r14));
    assembler->add(asmjit::x86::r14, 8); // Adjust return stack pointer (pop x2)
}


static void Compile_rFetch() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    // R@ ( -- x ) ( R: x -- x )
    assembler->comment("; --R@ ");
    assembler->sub(asmjit::x86::r15, 8); // Allocate space on data stack
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r15), asmjit::x86::r12); // Store TOS-1 in stack
    assembler->mov(asmjit::x86::r12, asmjit::x86::r13); // Move TOS to TOS-1
    assembler->mov(asmjit::x86::r13, asmjit::x86::ptr(asmjit::x86::r14)); // Copy return stack top into TOS
    assembler->comment("; End R@ ");
}

static void Compile_rpAt() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    // RP@ ( -- addr )
    assembler->comment("; --RP@ ");
    assembler->sub(asmjit::x86::r15, 8); // Allocate space on data stack
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r15), asmjit::x86::r13); // Store current TOS in stack
    assembler->mov(asmjit::x86::r13, asmjit::x86::r14); // Load return stack pointer into TOS
    assembler->comment("; End RP@ ");
}

static void Compile_rpStore() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    // RP! ( addr -- )
    assembler->comment("; --RP! ");
    assembler->mov(asmjit::x86::r14, asmjit::x86::r13); // Update R14 with new return stack pointer
    assembler->mov(asmjit::x86::r13, asmjit::x86::ptr(asmjit::x86::r15)); // Load new TOS from stack
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15, 8)); // Load new TOS-1 from stack
    assembler->add(asmjit::x86::r15, 8); // Adjust data stack pointer
    assembler->comment("; End RP! ");
}

// optional Return stack words
//
static void Compile_rDrop() {
    asmjit::x86::Assembler *assembler = &JitContext::instance().getAssembler();
    if (!assembler) {
        SignalHandler::instance().raise(10);
        return;
    }

    assembler->comment("; --RDROP ");
    assembler->add(asmjit::x86::r14, 8); // Adjust return stack pointer, dropping the top item
    assembler->comment("; End RDROP ");
}

static void Compile_r2Drop() {
    asmjit::x86::Assembler *assembler = &JitContext::instance().getAssembler();
    if (!assembler) {
        SignalHandler::instance().raise(10);
        return;
    }

    assembler->comment("; --R2DROP ");
    assembler->add(asmjit::x86::r14, 16); // Adjust return stack pointer to drop two values
    assembler->comment("; End R2DROP ");
}

static void Compile_rSwap() {
    asmjit::x86::Assembler *assembler = &JitContext::instance().getAssembler();
    if (!assembler) {
        SignalHandler::instance().raise(10);
        return;
    }

    assembler->comment("; --R>R ");
    assembler->mov(asmjit::x86::rax, asmjit::x86::ptr(asmjit::x86::r14)); // Load top of return stack into RAX
    assembler->mov(asmjit::x86::rbx, asmjit::x86::ptr(asmjit::x86::r14, 8)); // Load second element into RBX
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r14), asmjit::x86::rbx); // Swap: Store RBX at top
    assembler->mov(asmjit::x86::ptr(asmjit::x86::r14, 8), asmjit::x86::rax); // Swap: Store RAX in second position
    assembler->comment("; End R>R ");
}


void *depth() {
    const auto depth = static_cast<int64_t>((stack_top - fetchR15() > 0) ? ((stack_top - fetchR15()) / 8) : 0);
    cpush(depth);
    return nullptr;
}

void *rdepth() {
    const auto depth = static_cast<int64_t>((return_stack_top - fetchR14() > 0)
                                                ? ((return_stack_top - fetchR14()) / 8)
                                                : 0);
    cpush(depth);
    return nullptr;
}


// Used to build a working forth word, that can be executed by the compiler.
ForthFunction code_generator_build_forth(const ForthFunction fn) {
    // we need to start a new function
    ForthDictionary &dict = ForthDictionary::instance();

    code_generator_startFunction(dict.getLatestName());
    fn();
    compile_return();
    const auto f = reinterpret_cast<ForthFunction>(JitContext::instance().finalize());
    return f;
}


// add stack words to dictionary
void code_generator_add_stack_words() {
    ForthDictionary &dict = ForthDictionary::instance();


    // 2RDROP this is actually useful, given the DO LOOP indexes.
    // although it also one instruction...
    dict.addCodeWord("2RDROP", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&Compile_r2Drop),
                     code_generator_build_forth(Compile_r2Drop),
                     nullptr);


    // RDROP
    dict.addCodeWord("RDROP", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&Compile_rDrop),
                     code_generator_build_forth(Compile_rDrop),
                     nullptr);


    // R>R 'arrrrhhhharrrrrgggh'.
    dict.addCodeWord("R>R", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&Compile_rSwap),
                     code_generator_build_forth(Compile_rSwap),
                     nullptr);


    dict.addCodeWord("DEPTH", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     nullptr,
                     reinterpret_cast<ForthFunction>(depth),
                     nullptr);

    dict.addCodeWord("RDEPTH", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     nullptr,
                     reinterpret_cast<ForthFunction>(rdepth),
                     nullptr);


    // storeFromDS !
    dict.addCodeWord("!", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&storeFromDS),
                     code_generator_build_forth(storeFromDS),
                     nullptr);

    dict.addCodeWord("@", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&fetchFromDS),
                     code_generator_build_forth(fetchFromDS),
                     nullptr);


    // R@
    dict.addCodeWord("R@", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&Compile_rFetch),
                     code_generator_build_forth(Compile_rFetch),
                     nullptr);

    dict.addCodeWord("R@", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&Compile_rpAt),
                     code_generator_build_forth(Compile_rpAt),
                     nullptr);

    // RP!
    dict.addCodeWord("RP!", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&Compile_rpStore),
                     code_generator_build_forth(Compile_rpStore),
                     nullptr);

    // >R
    dict.addCodeWord(">R", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&Compile_toR),
                     code_generator_build_forth(Compile_toR),
                     nullptr
    );

    dict.addCodeWord("2>R", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&Compile_2toR),
                     code_generator_build_forth(Compile_2toR),
                     nullptr
    );

    dict.addCodeWord("R>", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&Compile_fromR),
                     code_generator_build_forth(Compile_fromR),
                     nullptr
    );

    dict.addCodeWord("2R>", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&Compile_2fromR),
                     code_generator_build_forth(Compile_2fromR),
                     nullptr
    );

    // DUP
    dict.addCodeWord("DUP", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_DUP),
                     code_generator_build_forth(compile_DUP),
                     nullptr);


    // DROP
    dict.addCodeWord("DROP", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_DROP),
                     code_generator_build_forth(compile_DROP),
                     nullptr);

    // 2DROP
    dict.addCodeWord("2DROP", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_2DROP),
                     code_generator_build_forth(compile_2DROP),
                     nullptr);

    // SWAP
    dict.addCodeWord("SWAP", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_SWAP),
                     code_generator_build_forth(compile_SWAP),
                     nullptr);

    // OVER
    dict.addCodeWord("OVER", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_OVER),
                     code_generator_build_forth(compile_OVER),
                     nullptr);

    // ROT
    dict.addCodeWord("ROT", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_ROT),
                     code_generator_build_forth(compile_ROT),
                     nullptr);

    // -ROT
    dict.addCodeWord("-ROT", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_MROT),
                     code_generator_build_forth(compile_MROT),
                     nullptr);

    // NIP
    dict.addCodeWord("NIP", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_NIP),
                     code_generator_build_forth(compile_NIP),
                     nullptr);

    // TUCK
    dict.addCodeWord("TUCK", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_TUCK),
                     code_generator_build_forth(compile_TUCK),
                     nullptr);

    // PICK
    dict.addCodeWord("PICK", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_PICK),
                     code_generator_build_forth(compile_PICK),
                     nullptr);

    // ROLL
    dict.addCodeWord("ROLL", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_ROLL),
                     code_generator_build_forth(compile_ROLL),
                     nullptr);

    // 2DUP
    dict.addCodeWord("2DUP", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_2DUP),
                     code_generator_build_forth(compile_2DUP),
                     nullptr);

    // 2OVER
    dict.addCodeWord("2OVER", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compiler_2OVER),
                     code_generator_build_forth(compiler_2OVER),
                     nullptr);

    // SP@
    dict.addCodeWord("SP@", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_AT),
                     code_generator_build_forth(compile_AT),
                     nullptr);

    // SP!
    dict.addCodeWord("SP!", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_SP_STORE),
                     code_generator_build_forth(compile_SP_STORE),
                     nullptr);
}


static void compile_ADD() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    assembler->comment("; -- ADD");

    assembler->add(asmjit::x86::r12, asmjit::x86::r13); // Add TOS (R13) to TOS-1 (R12)
    assembler->mov(asmjit::x86::r13, asmjit::x86::r12); // Promote TOS-1 to TOS
    assembler->add(asmjit::x86::r15, 8); // Adjust stack pointer to pop TOS
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Load new TOS-1 from memory
}

static void compile_SUB() {
    asmjit::x86::Assembler *assembler = &JitContext::instance().getAssembler();
    if (!assembler) {
        SignalHandler::instance().raise(10);
        return;
    }
    assembler->comment("; --SUB");

    assembler->sub(asmjit::x86::r12, asmjit::x86::r13); // Subtract TOS (R13) from TOS-1 (R12)
    assembler->mov(asmjit::x86::r13, asmjit::x86::r12); // Promote TOS-1 to TOS
    assembler->add(asmjit::x86::r15, 8); // Adjust stack pointer to pop TOS
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Load new TOS-1 from memory
}

static void compile_MUL() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    assembler->comment("; -- * MUL");

    assembler->imul(asmjit::x86::r12, asmjit::x86::r13); // Multiply TOS (R13) by TOS-1 (R12)
    compile_DROP(); // Drop TOS-1
}

static void compile_DIV() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    assembler->comment("; --DIV");

    assembler->mov(asmjit::x86::rax, asmjit::x86::r12); // Move TOS-1 (R12) into RAX
    assembler->cqo(); // Sign-extend RAX into RDX:RAX
    assembler->idiv(asmjit::x86::r13); // Divide RDX:RAX by TOS (R13)
    assembler->mov(asmjit::x86::r13, asmjit::x86::rax); // Move quotient (RAX) into TOS (R13)
    assembler->add(asmjit::x86::r15, 8); // Adjust stack pointer to pop TOS
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Load new TOS-1 from memory
}

static void compile_MOD() {
    asmjit::x86::Assembler *assembler = &JitContext::instance().getAssembler();
    if (!assembler) {
        SignalHandler::instance().raise(10);
        return;
    }
    assembler->comment("; --MOD");

    assembler->mov(asmjit::x86::rax, asmjit::x86::r12); // Move TOS-1 (R12) into RAX
    assembler->cqo(); // Sign-extend RAX into RDX:RAX
    assembler->idiv(asmjit::x86::r13); // Divide RDX:RAX by TOS (R13)
    assembler->mov(asmjit::x86::r13, asmjit::x86::rdx); // Move remainder (RDX) into TOS (R13)
    assembler->add(asmjit::x86::r15, 8); // Adjust stack pointer to pop TOS
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Load new TOS-1 from memory
}

static void compile_AND() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    assembler->comment("; --AND");

    assembler->and_(asmjit::x86::r12, asmjit::x86::r13); // Perform bitwise AND
    assembler->mov(asmjit::x86::r13, asmjit::x86::r12); // Promote TOS-1 to TOS
    assembler->add(asmjit::x86::r15, 8); // Adjust stack pointer to pop TOS
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Load new TOS-1 from memory
}

static void compile_OR() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    assembler->comment("; --OR");

    assembler->or_(asmjit::x86::r12, asmjit::x86::r13); // Perform bitwise OR
    assembler->mov(asmjit::x86::r13, asmjit::x86::r12); // Promote TOS-1 to TOS
    assembler->add(asmjit::x86::r15, 8); // Adjust stack pointer to pop TOS
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Load new TOS-1 from memory
}


static void compile_XOR() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    assembler->comment("; -- XOR");

    assembler->xor_(asmjit::x86::r12, asmjit::x86::r13); // Perform bitwise XOR between TOS (R13) and TOS-1 (R12)
    assembler->test(asmjit::x86::r12, asmjit::x86::r12); // Test if the result is zero
    assembler->setz(asmjit::x86::al); // Set AL to 1 if zero, 0 otherwise
    assembler->movzx(asmjit::x86::r12, asmjit::x86::al); // Zero-extend AL to R12 (R12 = 1 or 0)
    assembler->mov(asmjit::x86::r13, asmjit::x86::r12); // Copy the result to TOS (R13)
    assembler->add(asmjit::x86::r15, 8); // Adjust stack pointer to pop TOS
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Pull the new TOS-1 from memory
}

static void compile_NOT() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    assembler->comment("; -- NOT");

    assembler->test(asmjit::x86::r13, asmjit::x86::r13); // Test if TOS (R13) is zero
    assembler->setz(asmjit::x86::al); // Set AL to 1 if zero, 0 otherwise
    assembler->movzx(asmjit::x86::r13, asmjit::x86::al); // Zero-extend AL to R13 (R13 = 1 or 0)
}

static void compile_DIVMOD() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    assembler->comment("; -- DIVMOD /mod");

    assembler->mov(asmjit::x86::rax, asmjit::x86::r12); // Move TOS-1 (R12) into RAX (dividend)
    assembler->cqo(); // Sign-extend RAX into RDX:RAX
    assembler->idiv(asmjit::x86::r13); // Divide RDX:RAX by TOS (R13)
    assembler->mov(asmjit::x86::r12, asmjit::x86::rax); // Move quotient (RAX) into TOS-1 (R12)
    assembler->mov(asmjit::x86::r13, asmjit::x86::rdx); // Move remainder (RDX) into TOS (R13)
}


static void compile_SQRT() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    assembler->comment("; -- SQRT for integers using fp unit");
    assembler->cvtsi2sd(asmjit::x86::xmm0, asmjit::x86::r13);
    assembler->sqrtsd(asmjit::x86::xmm0, asmjit::x86::xmm0); // Compute sqrt(XMM0)
    assembler->cvttsd2si(asmjit::x86::r13, asmjit::x86::xmm0); // Truncate XMM0 (double) -> R12 (integer)
}


static void compile_SCALE() {
    // Get the assembler instance
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    assembler->comment("; --SCALE */ ");

    // Step 1: Load `a` (TOS-2) from [R15] into RAX
    assembler->mov(asmjit::x86::rax, asmjit::x86::ptr(asmjit::x86::r15)); // RAX = [R15] (TOS-2)

    // Step 2: Adjust the stack pointer to drop TOS-2 (increment R15 by 8 bytes)
    assembler->add(asmjit::x86::r15, 8); // R15 += 8 (move stack pointer upward)

    // Step 3: Multiply `a * b` (TOS-1 from R12)
    assembler->imul(asmjit::x86::rax, asmjit::x86::r12); // RAX = RAX * R12 (a * b)

    // Step 4: Sign-extend RAX into RDX:RAX prior to division
    assembler->cqo(); // Sign-extend RAX into RDX

    // Step 5: Divide `(a * b) / c` (where c is TOS in R13)
    assembler->idiv(asmjit::x86::r13); // RAX = RAX / R13 (result = (a * b) / c)

    // Step 6: Drop TOS (c is in R13) and update stack
    assembler->mov(asmjit::x86::r13, asmjit::x86::r12); // R13 = R12 (TOS = TOS-1)
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // R12 = [R15] (TOS-1 becomes TOS-2)
    assembler->add(asmjit::x86::r15, 8); // R15 += 8 (move stack pointer upward)

    // Step 7: Store the result (RAX) as the new TOS in R13
    assembler->mov(asmjit::x86::r13, asmjit::x86::rax); // R13 = RAX (Result)
}

static void compile_SCALEMOD() {
    // Retrieve the assembler.
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    assembler->comment("; -- SCALEMOD */MOD implementation");

    // Step 1: Load TOS-2 (a) into RAX from [R15]
    assembler->mov(asmjit::x86::rax, asmjit::x86::ptr(asmjit::x86::r15)); // RAX = [R15] (a)

    // Step 2: Adjust R15 (pop TOS-2 from stack)
    assembler->add(asmjit::x86::r15, 8); // R15 += 8 (drop TOS-2)

    // Step 3: Multiply TOS-2 (a) with TOS-1 (b in R12)
    assembler->imul(asmjit::x86::rax, asmjit::x86::r12); // RAX = RAX * R12 (a * b)

    // Step 4: Prepare RDX:RAX for signed division (c in R13 is the divisor)
    assembler->cqo(); // Sign-extend RAX into RDX:RAX

    // Step 5: Perform signed division: (a * b) / c
    assembler->idiv(asmjit::x86::r13); // Divide by c (R13), result in RAX (quotient), remainder in RDX

    // Step 6: Move the quotient (RAX) into TOS-1 (R12)
    assembler->mov(asmjit::x86::r13, asmjit::x86::rax); // R12 = Quotient

    // Step 7: Move the remainder (RDX) into TOS (R13)
    assembler->mov(asmjit::x86::r12, asmjit::x86::rdx); // R13 = Remainder
}


static void compile_LT() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    assembler->comment("; -- < (less than)");

    assembler->cmp(asmjit::x86::r12, asmjit::x86::r13); // Compare TOS (R13) with TOS-1 (R12)
    assembler->setl(asmjit::x86::al); // Set AL to 1 if TOS < TOS-1
    assembler->movzx(asmjit::x86::rax, asmjit::x86::al); // Zero-extend AL to RAX (ensure 0 or 1)
    assembler->neg(asmjit::x86::rax); // Negate RAX to produce -1 (true) or 0 (false)
    assembler->mov(asmjit::x86::r13, asmjit::x86::rax); // Store the result in TOS (R13)

    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Load TOS-3 into TOS-2 (R12)
    assembler->add(asmjit::x86::r15, 8); // Adjust stack pointer (R15) to reflect consumption of TOS-1
}


static void compile_GT() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    assembler->comment("; --> (greater than)");

    assembler->cmp(asmjit::x86::r12, asmjit::x86::r13);
    assembler->setg(asmjit::x86::al);
    assembler->movzx(asmjit::x86::rax, asmjit::x86::al);
    assembler->neg(asmjit::x86::rax);
    assembler->mov(asmjit::x86::r13, asmjit::x86::rax);

    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Load TOS-3 into TOS-2 (R12)
    assembler->add(asmjit::x86::r15, 8); // Adjust stack pointer (R15)
}

static void compile_LE() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    assembler->comment("; --<= (less than or equal to)");

    assembler->cmp(asmjit::x86::r12, asmjit::x86::r13);
    assembler->setle(asmjit::x86::al);
    assembler->movzx(asmjit::x86::rax, asmjit::x86::al);
    assembler->neg(asmjit::x86::rax);
    assembler->mov(asmjit::x86::r13, asmjit::x86::rax);

    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Load TOS-3 into TOS-2 (R12)
    assembler->add(asmjit::x86::r15, 8); // Adjust stack pointer (R15)
}


static void compile_EQ() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    assembler->comment("; -- = (equal)");

    assembler->cmp(asmjit::x86::r13, asmjit::x86::r12); // Compare TOS (R13) with TOS-1 (R12)
    assembler->sete(asmjit::x86::al); // Set AL to 1 if TOS == TOS-1
    assembler->movzx(asmjit::x86::rax, asmjit::x86::al); // Zero-extend AL to RAX (0 or 1)
    assembler->neg(asmjit::x86::rax); // Negate RAX to produce -1 (true) or 0 (false)
    assembler->mov(asmjit::x86::r13, asmjit::x86::rax); // Store the result in TOS (R13)

    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Load TOS-3 into TOS-2 (R12)
    assembler->add(asmjit::x86::r15, 8); // Adjust stack pointer
}

static void compile_NEQ() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    assembler->comment("; -- <> (not equal)");

    assembler->cmp(asmjit::x86::r13, asmjit::x86::r12); // Compare TOS (R13) with TOS-1 (R12)
    assembler->setne(asmjit::x86::al); // Set AL to 1 if TOS != TOS-1
    assembler->movzx(asmjit::x86::rax, asmjit::x86::al); // Zero-extend AL to RAX (0 or 1)
    assembler->neg(asmjit::x86::rax); // Negate RAX to produce -1 (true) or 0 (false)
    assembler->mov(asmjit::x86::r13, asmjit::x86::rax); // Store the result in TOS (R13)

    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Load TOS-3 into TOS-2 (R12)
    assembler->add(asmjit::x86::r15, 8); // Adjust stack pointer
}


void code_generator_add_operator_words() {
    ForthDictionary &dict = ForthDictionary::instance();

    dict.addCodeWord("=", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_EQ),
                     code_generator_build_forth(compile_EQ),
                     nullptr);

    dict.addCodeWord("<>", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_NEQ),
                     code_generator_build_forth(compile_NEQ),
                     nullptr);

    dict.addCodeWord("<", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_LT),
                     code_generator_build_forth(compile_LT),
                     nullptr);


    dict.addCodeWord(">", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_GT),
                     code_generator_build_forth(compile_GT),
                     nullptr);


    dict.addCodeWord("<=", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_LE),
                     code_generator_build_forth(compile_LE),
                     nullptr);

    dict.addCodeWord("/MOD", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_DIVMOD),
                     code_generator_build_forth(compile_DIVMOD),
                     nullptr);

    dict.addCodeWord("*/", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     nullptr,
                     code_generator_build_forth(compile_SCALE),
                     nullptr);


    dict.addCodeWord("*/MOD", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_SCALEMOD),
                     code_generator_build_forth(compile_SCALEMOD),
                     nullptr);


    dict.addCodeWord("SQRT", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_SQRT),
                     code_generator_build_forth(compile_SQRT),
                     nullptr);

    dict.addCodeWord("XOR", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_XOR),
                     code_generator_build_forth(compile_XOR),
                     nullptr);

    dict.addCodeWord("NOT", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_NOT),
                     code_generator_build_forth(compile_NOT),
                     nullptr);


    dict.addCodeWord("+", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_ADD),
                     code_generator_build_forth(compile_ADD),
                     nullptr);

    dict.addCodeWord("-", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_SUB),
                     code_generator_build_forth(compile_SUB),
                     nullptr
    );

    dict.addCodeWord("*", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_MUL),
                     code_generator_build_forth(compile_MUL),
                     nullptr);

    dict.addCodeWord("/", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_DIV),
                     code_generator_build_forth(compile_DIV),
                     nullptr);

    dict.addCodeWord("MOD", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_MOD),
                     code_generator_build_forth(compile_MOD),
                     nullptr);

    dict.addCodeWord("AND", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_AND),
                     code_generator_build_forth(compile_AND),
                     nullptr);

    dict.addCodeWord("OR", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_OR),
                     code_generator_build_forth(compile_OR),
                     nullptr);
}


// IO words


void code_generator_puts_no_crlf(const char *str) {
    std::cout << str;
}


// immediate interpreter words
// ."
void runImmediateString(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    const ForthToken first = tokens.front();
    if (first.type != TokenType::TOKEN_STRING) {
        SignalHandler::instance().raise(11);
        return;
    }
    code_generator_puts_no_crlf(first.value.c_str());
    tokens.erase(tokens.begin());
}


// CREATE creates a new WORD
void runImmediateCREATE(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    // when creating a new word a new symbol is expected.
    const ForthToken first = tokens.front();
    if (first.type != TokenType::TOKEN_UNKNOWN) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }

    auto &dict = ForthDictionary::instance();

    // Create the dictionary entry WITHOUT setting the executable first
    auto entry = dict.addCodeWord(
        first.value,
        "FORTH",
        ForthState::EXECUTABLE,
        ForthWordType::WORD,
        nullptr, // Placeholder - executable logic will be set later
        nullptr,
        nullptr);
    tokens.erase(tokens.begin()); // Remove the processed token

    const auto address = reinterpret_cast<uintptr_t>(&entry->executable);
    JitContext::instance().initialize();
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    assembler->commentf("; Push the words own address %lu", address);
    assembler->mov(asmjit::x86::rax, asmjit::imm(address));
    pushDS(asmjit::x86::rax);
    compile_return();

    const auto func = JitContext::instance().finalize();
    if (!func) {
        SignalHandler::instance().raise(12); // Error finalizing the JIT-compiled function
        return;
    }
    entry->executable = func;
}


void runImmediateVARIABLE(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    // when creating a new word a new symbol is expected.
    const ForthToken first = tokens.front();
    if (first.type != TokenType::TOKEN_UNKNOWN) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }
    tokens.erase(tokens.begin()); // Remove the processed token

    auto &dict = ForthDictionary::instance();

    // Create the dictionary entry WITHOUT setting the executable first
    const auto entry = dict.addCodeWord(
        first.value,
        "FORTH",
        ForthState::EXECUTABLE,
        ForthWordType::VARIABLE,
        nullptr, // Placeholder - executable logic will be set later
        nullptr,
        nullptr);

    // Allocate memory for the variable's data
    auto data_ptr = WordHeap::instance().allocate(entry->id, 16);
    if (!data_ptr || (reinterpret_cast<uintptr_t>(data_ptr) % 16 != 0)) {
        SignalHandler::instance().raise(3); // Invalid memory access
        return;
    }

    // Set the entry's data field to the allocated memory
    entry->data = data_ptr;

    code_generator_startFunction("NEW_VARIABLE");

    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    // Add runtime logic to resolve and push the variable's data pointer from entry->data
    assembler->commentf("; Push the variable's data address from entry->data using rbp");

    // Move the address of the dictionary entry into RAX
    assembler->mov(asmjit::x86::rax, asmjit::x86::ptr(asmjit::x86::rbp, offsetof(ForthDictionaryEntry, data)));
    // Push the resolved pointer onto the data stack
    pushDS(asmjit::x86::rax);

    // Add a NOP and RET for procedure alignment

    assembler->ret();

    const auto func = JitContext::instance().finalize();
    if (!func) {
        SignalHandler::instance().raise(12); // Error finalizing the JIT-compiled function
        return;
    }

    // Assign the finalized function to the dictionary entry's executable field
    entry->executable = func;
}

// default deferred word behaviour
static void defer_initial() {
    SignalHandler::instance().raise(13);
}

// DEFER creates a word with no assigned behaviour.
void runImmediateDEFER(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    // when creating a new word a new symbol is expected.
    const ForthToken first = tokens.front();
    if (first.type != TokenType::TOKEN_UNKNOWN) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }

    auto &dict = ForthDictionary::instance();

    // Create the dictionary entry WITHOUT setting the executable first
    dict.addCodeWord(
        first.value,
        "FORTH",
        ForthState::EXECUTABLE,
        ForthWordType::WORD,
        nullptr, // Placeholder - executable logic will be set later
        reinterpret_cast<ForthFunction>(defer_initial),
        nullptr);
    tokens.erase(tokens.begin()); // Remove the processed token
}


// IS assigns first words action to second words action
// IS new_action deferred_word
void runImmediateIS(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    // The first word is the new action
    const ForthToken first = tokens.front();
    if (first.type != TokenType::TOKEN_WORD) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }
    tokens.erase(tokens.begin()); // Remove the processed token

    // Get and remove the second token
    // The second word is the defer word, that will be updated.
    const ForthToken second = tokens.front();
    if (second.type != TokenType::TOKEN_WORD) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }
    tokens.erase(tokens.begin()); // Remove the processed token

    auto &dict = ForthDictionary::instance();
    auto first_word = dict.findWord(first.value.c_str());
    if (!first_word) {
        SignalHandler::instance().raise(14); // Invalid token - raise an error
        return;
    }
    auto second_word = dict.findWord(second.value.c_str());
    if (!second_word) {
        SignalHandler::instance().raise(14); // Invalid token - raise an error
        return;
    }

    if (first_word->executable != nullptr) {
        second_word->executable = first_word->executable;
    }
    if (first_word->generator != nullptr) {
        second_word->generator = first_word->generator;
    }
    if (first_word->immediate_interpreter != nullptr) {
        second_word->immediate_interpreter = first_word->immediate_interpreter;
    }
}

// Our Forth allocates data per word
// allot data on last word created
static void latest_word_allot_data() {
    const auto capacity = cpop(); // Pop the capacity from the stack
    const auto &dict = ForthDictionary::instance();
    dict.getLatestWordAdded()->data = WordHeap::instance().allocate(
        dict.getLatestWordAdded()->getID(),
        capacity);
}

// 512 ALLOT> word
void runImmediateALLOT_TO(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    // The first word is the new action
    const ForthToken first = tokens.front();
    if (first.type != TokenType::TOKEN_WORD) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }
    tokens.erase(tokens.begin()); // Remove the processed token

    auto &dict = ForthDictionary::instance();
    auto first_word = dict.findWord(first.value.c_str());
    if (!first_word) {
        SignalHandler::instance().raise(14); // Invalid token - raise an error
        return;
    }
    const auto capacity = cpop(); // Pop the capacity from the stack
    first_word->AllotData(capacity);
}

// show help displays all the show commands
void display_show_help() {
    std::cout << "usage show <topic>" << std::endl;
    std::cout << "available topics" << std::endl;
    std::cout << " words" << std::endl;
    std::cout << " chain" << std::endl;
    std::cout << " allot" << std::endl;
    std::cout << " strings" << std::endl;
    std::cout << " words_detailed" << std::endl;
}

// SHOW THING
void runImmediateSHOW(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    // The first word is the new action
    const ForthToken first = tokens.front();
    tokens.erase(tokens.begin()); // Remove the processed token
    auto thing = first.value;

    if (thing.empty()) {
        display_show_help();
        return;
    }
    if (thing == "ALLOT") {
        WordHeap::instance().listAllocations();
    } else if (thing == "CHAIN") {
        auto &dict = ForthDictionary::instance();
        for (int i = 0; i < 16; i++) {
            dict.displayWordChain(i);
        }
    } else if (thing == "STRINGS") {
        auto &stringStorage = StringStorage::instance();
        stringStorage.displayInternedStrings();
    } else if (thing == "WORDS") {
        auto &dict = ForthDictionary::instance();
        dict.displayWords();
    } else if (thing == "WORDS_DETAILED") {
        auto &dict = ForthDictionary::instance();
        for (int i = 0; i < 16; i++) {
            dict.displayDictionary();
        }
    } else {
    }
}


// introspection

void runImmediateSEE(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    // The first word is the new action
    const ForthToken first = tokens.front();
    if (first.type != TokenType::TOKEN_WORD) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }
    tokens.erase(tokens.begin()); // Remove the processed token

    auto &dict = ForthDictionary::instance();
    auto first_word = dict.findWord(first.value.c_str());
    if (!first_word) {
        SignalHandler::instance().raise(14); // Invalid token - raise an error
        return;
    }

    first_word->display();
}


// optimiser fragments

// add TOS by constant
void runImmediateADD_IMM(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    const ForthToken first = tokens.front();
    if (first.type != TokenType::TOKEN_OPTIMIZED) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }

    auto &dict = ForthDictionary::instance();
    auto first_word = dict.findWord(first.optimized_op.c_str());
    if (!first_word) {
        SignalHandler::instance().raise(14); // Invalid token - raise an error
        return;
    }
    // we will run the optimized generator here...
    //std::cout << "Running optimized generator:" << first_word->getWordName() << std::endl;

    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    assembler->commentf("; Add constant %llu", first.int_value);
    assembler->add(asmjit::x86::r13, asmjit::imm(first.int_value));
}

// CMP_LT_IMM - Compare if TOS (r13) is less than a constant
void runImmediateCMP_LT_IMM(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    const ForthToken first = tokens.front();
    if (first.type != TokenType::TOKEN_OPTIMIZED) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }

    auto &dict = ForthDictionary::instance();
    auto first_word = dict.findWord(first.optimized_op.c_str());
    if (!first_word) {
        SignalHandler::instance().raise(14); // Invalid token - raise an error
        return;
    }

    // Log or debugging output for tracing
    //std::cout << "Running optimized generator:" << first_word->getWordName() << std::endl;

    // Initialize the assembler
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    // Insert a helpful comment
    assembler->commentf("; Is TOS > %llu", first.int_value);

    // Perform comparison: Compare Top of Stack (TOS, r13) with a literal value
    assembler->cmp(asmjit::x86::r13, asmjit::imm(first.int_value));

    // Set the result (1 for true, 0 for false) into r13
    assembler->setb(asmjit::x86::al); // Set AL (lower byte of RAX) if "below" (unsigned less than)
    assembler->movzx(asmjit::x86::rax, asmjit::x86::al); // Zero-extend AL into r13 (TOS)
    assembler->neg(asmjit::x86::rax);

    assembler->mov(asmjit::x86::r13, asmjit::x86::rax);
}


// sub TOS by constant
void runImmediateSUB_IMM(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    // The first word is the new action
    const ForthToken first = tokens.front();
    if (first.type != TokenType::TOKEN_OPTIMIZED) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }


    auto &dict = ForthDictionary::instance();
    auto first_word = dict.findWord(first.optimized_op.c_str());
    if (!first_word) {
        SignalHandler::instance().raise(14); // Invalid token - raise an error
        return;
    }
    // we will run the optimized generator here...
    //std::cout << "Running optimized generator:" << first_word->getWordName() << std::endl;

    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    assembler->commentf("; Sub constant %llu", first.int_value);
    assembler->sub(asmjit::x86::r13, asmjit::imm(first.int_value));
}

// CMP_GT_IMM - Compare if TOS (r13) is greater than a constant and set result to -1 (true) or 0 (false)
void runImmediateCMP_GT_IMM(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    const ForthToken first = tokens.front();
    if (first.type != TokenType::TOKEN_OPTIMIZED) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }

    auto &dict = ForthDictionary::instance();
    auto first_word = dict.findWord(first.optimized_op.c_str());
    if (!first_word) {
        SignalHandler::instance().raise(14); // Invalid token - raise an error
        return;
    }

    // Log or debugging output for tracing
    //std::cout << "Running optimized generator:" << first_word->getWordName() << std::endl;

    // Initialize the assembler
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    // Insert a helpful comment
    assembler->commentf("; Is TOS > %llu", first.int_value);

    // Perform comparison: Compare TOS (r13) with the literal value
    assembler->cmp(asmjit::x86::r13, asmjit::imm(first.int_value));

    // Set AL to 1 if r13 > imm, otherwise 0
    assembler->seta(asmjit::x86::al); // "Set Above" for unsigned greater than

    // Move AL to TOS (r13) and extend it into a full register
    assembler->movzx(asmjit::x86::r13, asmjit::x86::al);

    // Negate TOS (invert `0` to `0` and `1` to `-1`)
    assembler->neg(asmjit::x86::r13);
}


// CMP_EQ_IMM - Compare if TOS (r13) is equal to a constant and set result to -1 (true) or 0 (false)
void runImmediateCMP_EQ_IMM(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    const ForthToken first = tokens.front();


    if (first.type != TokenType::TOKEN_OPTIMIZED) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }

    auto &dict = ForthDictionary::instance();
    auto first_word = dict.findWord(first.optimized_op.c_str());
    if (!first_word) {
        SignalHandler::instance().raise(14); // Invalid token - raise an error
        return;
    }

    // Log or debugging output for tracing
    //std::cout << "Running optimized generator:" << first_word->getWordName() << std::endl;

    // Initialize the assembler
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    // Insert a helpful comment
    assembler->commentf("; Compare TOS (r13) if equal to constant %llu", first.int_value);

    // Perform comparison: Compare TOS (r13) with the literal value
    assembler->cmp(asmjit::x86::r13, asmjit::imm(first.int_value));

    // Set AL to 1 if r13 == imm, otherwise 0
    assembler->sete(asmjit::x86::al); // "Set Equal"

    // Move AL to TOS (r13) and extend it into a full register
    assembler->movzx(asmjit::x86::r13, asmjit::x86::al);

    // Negate TOS (invert `0` to `0` and `1` to `-1`)
    assembler->neg(asmjit::x86::r13);
}


// shift left for powers of 2
void runImmediateSHL_IMM(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    // The first word is the new action
    const ForthToken first = tokens.front();
    if (first.type != TokenType::TOKEN_OPTIMIZED) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }


    auto &dict = ForthDictionary::instance();
    auto first_word = dict.findWord(first.optimized_op.c_str());
    if (!first_word) {
        SignalHandler::instance().raise(14); // Invalid token - raise an error
        return;
    }
    // we will run the optimized generator here...
    // std::cout << "Running optimized generator:" << first_word->getWordName() << std::endl;

    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    assembler->commentf("; Sub constant %llu", first.int_value);
    assembler->shl(asmjit::x86::r13, asmjit::imm(first.int_value));
}

// Shift right TOS by constant power of 2
void runImmediateSHR_IMM(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    // The first word is the new action
    const ForthToken first = tokens.front();
    if (first.type != TokenType::TOKEN_OPTIMIZED) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }


    auto &dict = ForthDictionary::instance();
    auto first_word = dict.findWord(first.optimized_op.c_str());
    if (!first_word) {
        SignalHandler::instance().raise(14); // Invalid token - raise an error
        return;
    }
    // we will run the optimized generator here...
    // std::cout << "Running optimized generator:" << first_word->getWordName() << std::endl;

    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    assembler->commentf("; Sub constant %llu", first.int_value);
    assembler->shr(asmjit::x86::r13, asmjit::imm(first.int_value));
}

// multiply TOS by immediate general
void runImmediateMUL_IMM(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    // The first word is the new action
    const ForthToken first = tokens.front();
    if (first.type != TokenType::TOKEN_OPTIMIZED) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }


    auto &dict = ForthDictionary::instance();
    auto first_word = dict.findWord(first.optimized_op.c_str());
    if (!first_word) {
        SignalHandler::instance().raise(14); // Invalid token - raise an error
        return;
    }
    // we will run the optimized generator here...
    //  std::cout << "Running optimized generator:" << first_word->getWordName() << std::endl;

    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    assembler->commentf("; IMUL by constant %llu", first.int_value);
    assembler->imul(asmjit::x86::r13, asmjit::x86::r13, asmjit::imm(first.int_value));
}

// multiply TOS by immediate general
void runImmediateDIV_IMM(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    // The first word is the new action
    const ForthToken first = tokens.front();
    if (first.type != TokenType::TOKEN_OPTIMIZED) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }


    auto &dict = ForthDictionary::instance();
    auto first_word = dict.findWord(first.optimized_op.c_str());
    if (!first_word) {
        SignalHandler::instance().raise(14); // Invalid token - raise an error
        return;
    }
    // we will run the optimized generator here...
    // std::cout << "Running optimized generator:" << first_word->getWordName() << std::endl;

    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    assembler->commentf("; IDIV by constant %llu", first.int_value);
    // Setup the dividend

    assembler->mov(asmjit::x86::rax, asmjit::x86::r13); // Move TOS (R13) to RAX.
    assembler->cdq(); // Sign-extend RAX into RDX:RAX.

    // Load the constant divisor
    assembler->mov(asmjit::x86::rcx, asmjit::imm(first.int_value)); // Load constant divisor into RCX.

    // Perform the division
    assembler->idiv(asmjit::x86::rcx); // Divide RDX:RAX by RCX. Quotient -> RAX, Remainder -> RDX.

    // Optionally store the result
    assembler->mov(asmjit::x86::r13, asmjit::x86::rax); // Move the quotient (RAX) back into TOS (R13).
}


// DUP + = lea r13, [r13 + r13] LEA_TOS
void runImmediateLEA_TOS(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    // The first word is the new action
    const ForthToken first = tokens.front();
    if (first.type != TokenType::TOKEN_OPTIMIZED) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }


    auto &dict = ForthDictionary::instance();
    auto first_word = dict.findWord(first.optimized_op.c_str());
    if (!first_word) {
        SignalHandler::instance().raise(14); // Invalid token - raise an error
        return;
    }

    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    assembler->commentf("; Optimized DUP + = lea r13, [r13 + r13]");
    assembler->lea(asmjit::x86::r13, asmjit::x86::ptr(asmjit::x86::r13, asmjit::x86::r13));
}

// safer alternative to VOCAB DEFINITIONS
void runImmediateSETCURRENT(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    // The first word is the new action
    const ForthToken first = tokens.front();
    if (first.type != TokenType::TOKEN_WORD) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }


    auto &dict = ForthDictionary::instance();
    auto first_word = dict.findWord(first.value.c_str());
    if (!first_word) {
        SignalHandler::instance().raise(14); // Invalid token - raise an error
        return;
    }
    //
    if (first_word->state == ForthState::EXECUTABLE
        && first_word->type == ForthWordType::VOCABULARY) {
        //
        dict.setVocabulary(first_word);
    }
}


// add immediate interpreter words.


void Forget() {
    ForthDictionary &dict = ForthDictionary::instance();
    dict.forgetLastWord();

}

void code_generator_add_immediate_words() {
    ForthDictionary &dict = ForthDictionary::instance();


    dict.addCodeWord("FORGET", "FORTH",
                     ForthState::IMMEDIATE,
                     ForthWordType::WORD,
                     nullptr,
                     Forget,
                     nullptr);


    dict.addCodeWord("SETCURRENT", "FORTH",
                     ForthState::IMMEDIATE,
                     ForthWordType::WORD,
                     nullptr,
                     nullptr,
                     runImmediateSETCURRENT);

    dict.addCodeWord("LEA_TOS", "FRAGMENTS",
                     ForthState::IMMEDIATE,
                     ForthWordType::MACRO,
                     nullptr,
                     nullptr,
                     runImmediateLEA_TOS);

    dict.addCodeWord("DIV_IMM", "FRAGMENTS",
                     ForthState::IMMEDIATE,
                     ForthWordType::MACRO,
                     nullptr,
                     nullptr,
                     runImmediateDIV_IMM);


    dict.addCodeWord("CMP_GT_IMM", "FRAGMENTS",
                     ForthState::IMMEDIATE,
                     ForthWordType::MACRO,
                     nullptr,
                     nullptr,
                     runImmediateCMP_GT_IMM);

    dict.addCodeWord("CMP_LT_IMM", "FRAGMENTS",
                     ForthState::IMMEDIATE,
                     ForthWordType::MACRO,
                     nullptr,
                     nullptr,
                     runImmediateCMP_LT_IMM);

    dict.addCodeWord("CMP_EQ_IMM", "FRAGMENTS",
                     ForthState::IMMEDIATE,
                     ForthWordType::MACRO,
                     nullptr,
                     nullptr,
                     runImmediateCMP_EQ_IMM);

    dict.addCodeWord("MUL_IMM", "FRAGMENTS",
                     ForthState::IMMEDIATE,
                     ForthWordType::MACRO,
                     nullptr,
                     nullptr,
                     runImmediateMUL_IMM);

    dict.addCodeWord("SHR_IMM", "FRAGMENTS",
                     ForthState::IMMEDIATE,
                     ForthWordType::MACRO,
                     nullptr,
                     nullptr,
                     runImmediateSHR_IMM);

    dict.addCodeWord("SHL_IMM", "FRAGMENTS",
                     ForthState::IMMEDIATE,
                     ForthWordType::MACRO,
                     nullptr,
                     nullptr,
                     runImmediateSHL_IMM);

    dict.addCodeWord("SUB_IMM", "FRAGMENTS",
                     ForthState::IMMEDIATE,
                     ForthWordType::MACRO,
                     nullptr,
                     nullptr,
                     runImmediateSUB_IMM);


    dict.addCodeWord("ADD_IMM", "FRAGMENTS",
                     ForthState::IMMEDIATE,
                     ForthWordType::MACRO,
                     nullptr,
                     nullptr,
                     runImmediateADD_IMM);


    dict.addCodeWord("SET", "FORTH",
                     ForthState::IMMEDIATE,
                     ForthWordType::WORD,
                     nullptr,
                     nullptr,
                     runImmediateSET);

    dict.addCodeWord("SHOW", "FORTH",
                     ForthState::IMMEDIATE,
                     ForthWordType::WORD,
                     nullptr,
                     nullptr,
                     runImmediateSHOW);


    dict.addCodeWord("SEE", "FORTH",
                     ForthState::IMMEDIATE,
                     ForthWordType::WORD,
                     nullptr,
                     nullptr,
                     runImmediateSEE);

    dict.addCodeWord("ALLOT", "FORTH",
                     ForthState::IMMEDIATE,
                     ForthWordType::WORD,
                     nullptr,
                     latest_word_allot_data,
                     nullptr);


    dict.addCodeWord("ALLOT>", "FORTH",
                     ForthState::IMMEDIATE,
                     ForthWordType::WORD,
                     nullptr,
                     nullptr,
                     runImmediateALLOT_TO);


    dict.addCodeWord("CREATE", "FORTH",
                     ForthState::IMMEDIATE,
                     ForthWordType::WORD,
                     nullptr,
                     nullptr,
                     runImmediateCREATE);

    dict.addCodeWord("VARIABLE", "FORTH",
                     ForthState::IMMEDIATE,
                     ForthWordType::WORD,
                     nullptr,
                     nullptr,
                     runImmediateVARIABLE);


    dict.addCodeWord("DEFER", "FORTH",
                     ForthState::IMMEDIATE,
                     ForthWordType::WORD,
                     nullptr,
                     nullptr,
                     runImmediateDEFER);

    dict.addCodeWord("IS", "FORTH",
                     ForthState::IMMEDIATE,
                     ForthWordType::WORD,
                     nullptr,
                     nullptr,
                     runImmediateIS);
}


// IO words

void spit_str(const char *str) {
    std::cout << str << std::flush;
}


static void spit_number(const int64_t n) {
    std::cout << n << ' ';
}

static void spit_number_f(double f) {
    std::cout << f << ' ';
}


static void spit_char(const char c) {
    std::cout << c;
}

static void spit_end_line() {
    std::cout << std::endl;
}

static void spit_cls() {
    std::cout << "\033c";
}

static void compile_DOT() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    // DROP ( x -- )
    assembler->comment("; -- DOT ");

    assembler->push(asmjit::x86::rdi); // Push TOS onto the stack
    assembler->mov(asmjit::x86::rdi, asmjit::x86::r13); // TOS
    assembler->comment("; call spit_number ");
    assembler->call(spit_number);
    assembler->pop(asmjit::x86::rdi); // Pop TOS off the stack

    assembler->mov(asmjit::x86::r13, asmjit::x86::r12); // Move TOS-1 into TOS
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Move TOS-2 into TOS-1
    assembler->add(asmjit::x86::r15, 8); // Adjust stack pointer
}


static void compile_DotString(std::deque<ForthToken> &tokens) {
    if (tokens.empty()) return; // Exit early if no tokens to process

    // Get and remove the first token
    const ForthToken first = tokens.front();
    if (first.type != TokenType::TOKEN_WORD && first.value != ".\"") {
        SignalHandler::instance().raise(11);
        return;
    }

    tokens.erase(tokens.begin()); // Remove the processed token
    if (tokens.empty()) return; // Exit early if no string token to process
    // Get the second token
    const ForthToken second = tokens.front();
    if (second.type != TokenType::TOKEN_STRING) {
        SignalHandler::instance().raise(11); // Invalid token - raise an error
        return;
    }


    // we need to save the string literal
    auto &stringStorage = StringStorage::instance();
    const char *addr1 = stringStorage.intern(second.value);

    // std::cout << std::endl << "string address: " << std::hex
    // << reinterpret_cast<const void*>(addr1)
    // << std::dec << std::endl;
    // std::cout << "string length: " << second.value.length() << std::endl;
    // std::cout << "string value: " << addr1 << std::endl;

    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    code_generator_align(assembler);
    assembler->comment("; -- dot string ");
    assembler->push(asmjit::x86::rdi);
    assembler->comment("; -- address of interned string");
    assembler->mov(asmjit::x86::rdi, asmjit::imm(addr1));
    assembler->comment("; call spit string ");
    assembler->call(spit_str);
    assembler->pop(asmjit::x86::rdi); // Pop TOS off the stack
}


static void compile_EMIT() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    // DROP ( x -- )
    assembler->comment("; -- EMIT ");

    assembler->push(asmjit::x86::rdi); // Push TOS onto the stack
    assembler->mov(asmjit::x86::rdi, asmjit::x86::r13); // TOS
    assembler->comment("; call spit_char");
    assembler->call(spit_char);
    assembler->pop(asmjit::x86::rdi); // Pop TOS off the stack

    assembler->mov(asmjit::x86::r13, asmjit::x86::r12); // Move TOS-1 into TOS
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Move TOS-2 into TOS-1
    assembler->add(asmjit::x86::r15, 8); // Adjust stack pointer
}


static void compile_CR() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    // DROP ( x -- )
    assembler->comment("; -- CR ");

    assembler->push(asmjit::x86::rdi); // Push TOS onto the stack
    assembler->mov(asmjit::x86::rdi, asmjit::x86::r13); // TOS
    assembler->comment("; call spit end line (CR)");
    assembler->call(spit_end_line);
    assembler->pop(asmjit::x86::rdi); // Pop TOS off the stack
}


static void compile_SPACE() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    // DROP ( x -- )
    assembler->comment("; -- SPACE ");

    assembler->push(asmjit::x86::rdi); // Push TOS onto the stack
    assembler->mov(asmjit::x86::rdi, asmjit::imm(32)); // TOS
    assembler->comment("; call spit_char with space ");
    assembler->call(spit_char);
    assembler->pop(asmjit::x86::rdi); // Pop TOS off the stack
}

static void compile_CLS() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    // DROP ( x -- )
    assembler->comment("; -- CLS ");

    assembler->push(asmjit::x86::rdi); // Push TOS onto the stack
    assembler->mov(asmjit::x86::rdi, asmjit::imm(32)); // TOS
    assembler->comment("; send clear screen esc c");
    assembler->call(spit_cls);
    assembler->pop(asmjit::x86::rdi); // Pop TOS off the stack
}


static void compile_PAGE() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    // DROP ( x -- )
    assembler->comment("; -- PAGE ");

    assembler->push(asmjit::x86::rdi); // Push TOS onto the stack
    assembler->mov(asmjit::x86::rdi, asmjit::imm(12)); // TOS
    assembler->comment("; call spit_char with page (12) ");
    assembler->call(spit_char);
    assembler->pop(asmjit::x86::rdi); // Pop TOS off the stack

    assembler->mov(asmjit::x86::r13, asmjit::x86::r12); // Move TOS-1 into TOS
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Move TOS-2 into TOS-1
    assembler->add(asmjit::x86::r15, 8); // Adjust stack pointer
}


void code_generator_add_io_words() {
    ForthDictionary &dict = ForthDictionary::instance();


    dict.addCodeWord(".\"", "FORTH",
                     ForthState::IMMEDIATE,
                     ForthWordType::WORD,
                     nullptr,
                     nullptr,
                     runImmediateString,
                     &compile_DotString);


    dict.addCodeWord(".", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_DOT),
                     code_generator_build_forth(compile_DOT),
                     nullptr);


    dict.addCodeWord("SPACE", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_SPACE),
                     code_generator_build_forth(compile_SPACE),
                     nullptr);

    dict.addCodeWord("PAGE", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_PAGE),
                     code_generator_build_forth(compile_PAGE),
                     nullptr);

    dict.addCodeWord("CLS", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_CLS),
                     code_generator_build_forth(compile_CLS),
                     nullptr);

    dict.addCodeWord("CR", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_CR),
                     code_generator_build_forth(compile_CR),
                     nullptr);


    dict.addCodeWord("EMIT", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&compile_EMIT),
                     code_generator_build_forth(compile_EMIT),
                     nullptr);
}

// FORTH DEFINITIONS set current directory to FORTH

ForthFunction execDEFINITIONS() {
    std::cout << "Executing DEFINITION" << std::endl;
    auto *entry = reinterpret_cast<ForthDictionaryEntry *>(cpop());
    if (!isHeapPointer(entry, code_generator_heap_start)) {
        SignalHandler::instance().raise(18);
    }
    entry->display();
    ForthDictionary::instance().setVocabulary(entry);
    ForthDictionary::instance().setVocabulary(SymbolTable::instance().getSymbol(entry->id));

    return nullptr;
}


void code_generator_add_vocab_words() {
    ForthDictionary &dict = ForthDictionary::instance();

    // FORTH DEFINITIONS
    dict.addCodeWord("DEFINITIONS", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     nullptr,
                     reinterpret_cast<ForthFunction>(&execDEFINITIONS),
                     nullptr
    );
}

static void genExit() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    assembler->comment("; - EXIT ");

    if (doLoopDepth > 0) {
        assembler->comment("; -- adjust forth return stack ");
        const auto drop_bytes = 8 * doLoopDepth;
        assembler->add(asmjit::x86::r14, drop_bytes);
    }

    labels.jmp(*assembler, "exit_label");
}


static void genDo() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    //
    assembler->comment("; -- DO (start of LOOP)");
    assembler->comment("; -- ");

    Compile_2toR(); // move loop,index to RS

    // Increment the DO loop depth counter
    doLoopDepth++;

    // Create labels for loop start and end

    assembler->comment("; -- DO label");

    DoLoopLabel doLoopLabel;
    doLoopLabel.doLabel = assembler->newLabel();
    doLoopLabel.loopLabel = assembler->newLabel();
    doLoopLabel.leaveLabel = assembler->newLabel();
    doLoopLabel.hasLeave = false;
    assembler->bind(doLoopLabel.doLabel);

    // Create a LoopLabel struct and push it onto the unified loopStack
    LoopLabel loopLabel;
    loopLabel.type = LoopType::DO_LOOP;
    loopLabel.label = doLoopLabel;

    loopStack.push(loopLabel);
}


static void genLoop() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    assembler->comment("; -- LOOP");
    // check if loopStack is empty
    if (loopStack.empty())
        throw std::runtime_error("gen_loop: loopStack is empty");

    const auto loopLabelVariant = loopStack.top();
    loopStack.pop(); // We are at the end of the loop.

    if (loopLabelVariant.type != LoopType::DO_LOOP)
        throw std::runtime_error("gen_loop: Current loop is not a DO loop");

    const auto &loopLabel = std::get<DoLoopLabel>(loopLabelVariant.label);


    assembler->comment("; -- LOOP index=rcx, limit=rdx");


    asmjit::x86::Gp currentIndex = asmjit::x86::rcx; // Current index
    asmjit::x86::Gp limit = asmjit::x86::rdx; // Limit


    popRS(currentIndex);
    popRS(limit);
    assembler->comment("; Push limit back");
    pushRS(limit);

    assembler->comment("; Increment index");
    assembler->add(currentIndex, 1);

    // Push the updated index back onto RS
    assembler->comment("; Push index back");
    pushRS(currentIndex);

    assembler->comment("; compare index, limit");
    // Check if current index is less than limit
    assembler->cmp(currentIndex, limit);

    assembler->comment("; Jump back or exit");
    // Jump to loop start if current index is less than the limit

    assembler->jl(loopLabel.doLabel);

    assembler->comment("; -- LOOP label");
    assembler->bind(loopLabel.loopLabel);
    assembler->comment("; -- LEAVE label");
    assembler->bind(loopLabel.leaveLabel);

    // Drop the current index and limit from the return stack
    assembler->comment("; -- drop loop counters");
    Compile_r2Drop();


    // Decrement the DO loop depth counter
    doLoopDepth--;
}

static void genPlusLoop() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    assembler->comment("; -- +LOOP");
    // check if loopStack is empty
    if (loopStack.empty())
        throw std::runtime_error("gen_loop: loopStack is empty");

    const auto loopLabelVariant = loopStack.top();
    loopStack.pop(); // We are at the end of the loop.

    if (loopLabelVariant.type != LoopType::DO_LOOP)
        throw std::runtime_error("gen_loop: Current loop is not a DO loop");

    const auto &loopLabel = std::get<DoLoopLabel>(loopLabelVariant.label);


    assembler->comment("; -- LOOP index=rcx, limit=rdx");


    asmjit::x86::Gp currentIndex = asmjit::x86::rcx; // Current index
    asmjit::x86::Gp limit = asmjit::x86::rdx; // Limit

    popRS(currentIndex);
    popRS(limit);
    assembler->comment("; Push limit back");
    pushRS(limit);

    assembler->comment("; Increment index");
    assembler->add(currentIndex, asmjit::x86::r13); // TOS
    compile_DROP();

    // Push the updated index back onto RS
    assembler->comment("; Push index back");
    pushRS(currentIndex);

    assembler->comment("; compare index, limit");
    // Check if current index is less than limit
    assembler->cmp(currentIndex, limit);

    assembler->comment("; Jump back or exit");
    // Jump to loop start if current index is less than the limit

    assembler->jl(loopLabel.doLabel);

    assembler->comment("; -- LOOP label");
    assembler->bind(loopLabel.loopLabel);
    assembler->comment("; -- LEAVE label");
    assembler->bind(loopLabel.leaveLabel);

    // Drop the current index and limit from the return stack
    assembler->comment("; -- drop loop counters");
    Compile_r2Drop();


    // Decrement the DO loop depth counter
    doLoopDepth--;
}


static void genI() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    //
    assembler->comment("; -- I (gets loop counter)");
    // Check if there is at least one loop counter on the unified stack
    if (doLoopDepth == 0) {
        throw std::runtime_error("gen_I: No matching DO_LOOP structure on the stack");
    }
    // Load the innermost loop index (top of the RS)
    assembler->comment("; -- making room");
    compile_DUP();
    assembler->mov(asmjit::x86::r13, asmjit::x86::ptr(asmjit::x86::r14));
    assembler->comment("; -- I index to TOS");
}


static void genJ() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    if (doLoopDepth < 2) {
        throw std::runtime_error("gen_j: Not enough nested DO-loops available");
    }

    assembler->comment("; -- making room");
    compile_DUP();
    // Offset for depth - 2 for index
    assembler->mov(asmjit::x86::r13, asmjit::x86::ptr(asmjit::x86::r14, 2 * 8));
    assembler->comment("; -- J index to TOS");
}


static void genK() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    if (doLoopDepth < 3) {
        throw std::runtime_error("gen_j: Not enough nested DO-loops available");
    }

    assembler->comment("; -- making room");
    compile_DUP();
    // Offset for depth - 2 for index
    assembler->mov(asmjit::x86::r13, asmjit::x86::ptr(asmjit::x86::r14, 4 * 8));
    assembler->comment("; -- J index to TOS");
}


static void genLeave() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    //
    assembler->comment("; -- leave");


    if (loopStack.empty()) {
        throw std::runtime_error("gen_leave: No loop to leave from");
    }

    // Save current state of loop stack to temp stack
    saveStackToTemp();

    bool found = false;
    asmjit::Label targetLabel;

    std::stack<LoopLabel> workingStack = tempLoopStack;

    // Search for the appropriate leave label in the temporary stack
    while (!workingStack.empty()) {
        LoopLabel topLabel = workingStack.top();
        workingStack.pop();

        switch (topLabel.type) {
            case DO_LOOP: {
                const auto &loopLabel = std::get<DoLoopLabel>(topLabel.label);
                targetLabel = loopLabel.leaveLabel;
                found = true;
                assembler->comment("; Jumps to do loop's leave label");
                break;
            }
            case BEGIN_AGAIN_REPEAT_UNTIL: {
                const auto &loopLabel = std::get<BeginAgainRepeatUntilLabel>(topLabel.label);
                targetLabel = loopLabel.leaveLabel;
                found = true;
                assembler->comment("; Jumps to begin/again/repeat/until leave label");
                break;
            }
            default:
                // Continue to look for the correct label
                break;
        }

        if (found) {
            break;
        }
    }

    if (!found) {
        throw std::runtime_error("gen_leave: No valid loop label found");
    }

    // Reconstitute the temporary stack back into the loopStack
    restoreStackFromTemp();

    // Jump to the found leave label
    assembler->jmp(targetLabel);
}


static void genBegin() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    assembler->comment("; -- BEGIN ");

    BeginAgainRepeatUntilLabel beginLabel;
    // Create all possible labels here.
    beginLabel.beginLabel = assembler->newLabel();
    beginLabel.untilLabel = assembler->newLabel(); // also repeat
    beginLabel.againLabel = assembler->newLabel();
    beginLabel.whileLabel = assembler->newLabel();
    beginLabel.leaveLabel = assembler->newLabel();

    assembler->comment("; LABEL for BEGIN");
    assembler->bind(beginLabel.beginLabel);

    // Push the new label struct onto the unified stack
    loopStack.push({BEGIN_AGAIN_REPEAT_UNTIL, beginLabel});
}

static void genUntil() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    if (loopStack.empty() || loopStack.top().type != BEGIN_AGAIN_REPEAT_UNTIL) {
        throw std::runtime_error("gen_until: No matching BEGIN_AGAIN_REPEAT_UNTIL structure on the stack");
    }


    //
    assembler->comment("; -- UNTIL");

    // Get the label from the unified stack
    const auto &beginLabels = std::get<BeginAgainRepeatUntilLabel>(loopStack.top().label);

    asmjit::x86::Gp topOfStack = asmjit::x86::rax;
    popDS(topOfStack);

    assembler->comment("; Jump back if zero");
    assembler->test(topOfStack, topOfStack);
    assembler->jz(beginLabels.beginLabel);

    assembler->comment("; LABEL for REPEAT/UNTIL");
    // Bind the appropriate labels
    assembler->bind(beginLabels.untilLabel);

    assembler->comment("; LABEL for LEAVE");
    assembler->bind(beginLabels.leaveLabel);

    // Pop the stack element as we're done with this construct
    loopStack.pop();
}


static void genAgain() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    if (loopStack.empty() || loopStack.top().type != BEGIN_AGAIN_REPEAT_UNTIL) {
        throw std::runtime_error("gen_again: No matching BEGIN_AGAIN_REPEAT_UNTIL structure on the stack");
    }


    //
    assembler->comment("; -- AGAIN");

    auto beginLabels = std::get<BeginAgainRepeatUntilLabel>(loopStack.top().label);
    loopStack.pop();


    beginLabels.againLabel = assembler->newLabel();
    assembler->jmp(beginLabels.beginLabel);


    assembler->comment("; LABEL for AGAIN");
    assembler->bind(beginLabels.againLabel);

    assembler->comment("; LABEL for LEAVE");
    assembler->bind(beginLabels.leaveLabel);


    assembler->comment("; LABEL for WHILE");
    assembler->bind(beginLabels.whileLabel);
}


static void genRepeat() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    if (loopStack.empty() || loopStack.top().type != BEGIN_AGAIN_REPEAT_UNTIL) {
        throw std::runtime_error("gen_repeat: No matching BEGIN_AGAIN_REPEAT_UNTIL structure on the stack");
    }

    auto beginLabels = std::get<BeginAgainRepeatUntilLabel>(loopStack.top().label);
    loopStack.pop();
    assembler->comment("; WHILE body end   --- ");
    assembler->comment("; -- REPEAT");
    // assembler->comment("; LABEL for REPEAT");
    beginLabels.repeatLabel = assembler->newLabel();
    assembler->comment("; Jump to BEGIN");
    assembler->jmp(beginLabels.beginLabel);
    assembler->bind(beginLabels.repeatLabel);

    assembler->comment("; LABEL for LEAVE");
    assembler->bind(beginLabels.leaveLabel);

    assembler->comment("; LABEL after REPEAT");

    assembler->bind(beginLabels.whileLabel);
}


static void genWhile() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //
    if (loopStack.empty() || loopStack.top().type != BEGIN_AGAIN_REPEAT_UNTIL) {
        throw std::runtime_error("gen_while: No matching BEGIN_AGAIN_REPEAT_UNTIL structure on the stack");
    }


    //
    assembler->comment("; -- WHILE ");


    auto beginLabel = std::get<BeginAgainRepeatUntilLabel>(loopStack.top().label);
    asmjit::x86::Gp topOfStack = asmjit::x86::rax;
    popDS(topOfStack);

    assembler->comment("; check if zero");
    assembler->test(topOfStack, topOfStack);
    assembler->comment("; if zero jump past REPEAT");
    assembler->jz(beginLabel.whileLabel);
    assembler->comment("; WHILE body --- start ");
}

// Test for while
// SET LOGGING ON
// : TW 1 BEGIN DUP 10 < WHILE DUP . 1 + REPEAT DROP ;
/*
align 16
; -- enter function: TW2
; -- enter_function
L0:
L2:
; ----- RBP is set to dictionary entry
mov rax, 0x600000223980                     ; 48B88039220000600000
mov rbp, rax                                ; 4889C5
; -- LITERAL (make space)
sub r15, 8                                  ; 4983EF08
mov [r15], r12                              ; 4D8927
mov r12, r13                                ; 4D89EC
mov r13, 1                                  ; 49C7C501000000
; -- TOS is 1

; -- BEGIN
; LABEL for BEGIN
L4:
; -- DUP
sub r15, 8                                  ; 4983EF08
mov [r15], r12                              ; 4D8927
mov r12, r13                                ; 4D89EC
; Is TOS > 10
cmp r13, 0xA                                ; 4983FD0A
setb al                                     ; 0F92C0
movzx rax, al                               ; 480FB6C0
neg rax                                     ; 48F7D8
mov r13, rax                                ; 4989C5
; -- DUP
sub r15, 8                                  ; 4983EF08
mov [r15], r12                              ; 4D8927
mov r12, r13                                ; 4D89EC
; -- DOT
push rdi                                    ; 57
mov rdi, r13                                ; 4C89EF
; call spit_number
call 0x104031150                            ; 40E800000000
pop rdi                                     ; 5F
mov r13, r12                                ; 4D89E5
mov r12, [r15]                              ; 4D8B27
add r15, 8                                  ; 4983C708
; Add constant 1
add r13, 1                                  ; 4983C501
; WHILE body end   ---
; -- REPEAT
; Jump to BEGIN
short jmp L4                                ; EBC0
L9:
; LABEL for LEAVE
L8:
; LABEL after REPEAT
L7:
; -- DROP
mov r13, r12                                ; 4D89E5
mov r12, [r15]                              ; 4D8B27
add r15, 8                                  ; 4983C708
; -- exit_label
L1:
ret                                         ; C3
; end of -- TW2 --                                    ; C3
; end of -- TW --
*/


static void genRedo() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    // Look for the current function's entry label on the loop stack
    if (!loopStack.empty() && loopStack.top().type == FUNCTION_ENTRY_EXIT) {
        auto functionLabels = std::get<FunctionEntryExitLabel>(loopStack.top().label);

        assembler->comment("; -- REDO (jump to start of word) ");

        // Generate a call to the entry label (self-recursion)
        assembler->jmp(functionLabels.entryLabel);
    } else {
        throw std::runtime_error("genRedo: No matching FUNCTION_ENTRY_EXIT structure on the stack");
    }
}


// recursion
static void genRecurse() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    // Look for the current function's entry label on the loop stack
    if (!loopStack.empty() && loopStack.top().type == FUNCTION_ENTRY_EXIT) {
        auto functionLabels = std::get<FunctionEntryExitLabel>(loopStack.top().label);

        assembler->comment("; -- RECURSE (call self)");
        // Generate a call to the entry label (self-recursion)
        assembler->push(asmjit::x86::rdi);
        assembler->call(functionLabels.entryLabel);
        assembler->pop(asmjit::x86::rdi);
    } else {
        throw std::runtime_error("genRecurse: No matching FUNCTION_ENTRY_EXIT structure on the stack");
    }
}

static void genIf() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);


    IfThenElseLabel branches;
    branches.ifLabel = assembler->newLabel();
    branches.elseLabel = assembler->newLabel();
    branches.thenLabel = assembler->newLabel();
    branches.leaveLabel = assembler->newLabel();
    branches.exitLabel = assembler->newLabel();
    branches.hasElse = false;
    branches.hasLeave = false;
    branches.hasExit = false;

    // Push the new IfThenElseLabel structure onto the unified loopStack
    loopStack.push({IF_THEN_ELSE, branches});

    assembler->comment("; -- IF ");


    // Pop the condition flag from the data stack
    asmjit::x86::Gp flag = asmjit::x86::rax;
    popDS(flag);

    // Conditional jump to either the ELSE or THEN location
    assembler->comment("; 0 branch to ELSE or THEN");
    assembler->test(flag, flag);
    assembler->jz(branches.ifLabel);
}


static void genElse() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    assembler->comment("; -- ELSE ");

    if (!loopStack.empty() && loopStack.top().type == IF_THEN_ELSE) {
        auto branches = std::get<IfThenElseLabel>(loopStack.top().label);
        assembler->comment("; jump past ELSE");
        assembler->jmp(branches.elseLabel); // Jump to the code after the ELSE block
        assembler->comment("; ----- label for ELSE");
        assembler->bind(branches.ifLabel);
        branches.hasElse = true;

        // Update the stack with the modified branches
        loopStack.pop();
        loopStack.push({IF_THEN_ELSE, branches});
    } else {
        throw std::runtime_error("genElse: No matching IF_THEN_ELSE structure on the stack");
    }
}

static void genThen() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);


    if (!loopStack.empty() && loopStack.top().type == IF_THEN_ELSE) {
        auto branches = std::get<IfThenElseLabel>(loopStack.top().label);
        if (branches.hasElse) {
            assembler->comment("; ELSE label ");
            assembler->bind(branches.elseLabel); // Bind the ELSE label
        } else if (branches.hasLeave) {
            assembler->comment("; LEAVE label ");
            assembler->bind(branches.leaveLabel); // Bind the leave label
        } else if (branches.hasExit) {
            assembler->comment("; EXIT label ");
            assembler->bind(branches.exitLabel); // Bind the exit label
        } else {
            assembler->comment("; THEN label ");
            assembler->bind(branches.ifLabel);
        }
        loopStack.pop();
    } else {
        throw std::runtime_error("genThen: No matching IF_THEN_ELSE structure on the stack");
    }
}

void code_generator_add_control_flow_words() {
    ForthDictionary &dict = ForthDictionary::instance();


    dict.addCodeWord("EXIT", "FORTH",
                     ForthState::GENERATOR,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genExit),
                     nullptr,
                     nullptr);


    dict.addCodeWord("THEN", "FORTH",
                     ForthState::GENERATOR,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genThen),
                     nullptr,
                     nullptr);

    dict.addCodeWord("IF", "FORTH",
                     ForthState::GENERATOR,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genIf),
                     nullptr,
                     nullptr);

    dict.addCodeWord("ELSE", "FORTH",
                     ForthState::GENERATOR,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genElse),
                     nullptr,
                     nullptr);


    dict.addCodeWord("BEGIN", "FORTH",
                     ForthState::GENERATOR,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genBegin),
                     nullptr,
                     nullptr);

    dict.addCodeWord("AGAIN", "FORTH",
                     ForthState::GENERATOR,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genAgain),
                     nullptr,
                     nullptr);

    dict.addCodeWord("WHILE", "FORTH",
                     ForthState::GENERATOR,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genWhile),
                     nullptr,
                     nullptr);

    dict.addCodeWord("REPEAT", "FORTH",
                     ForthState::GENERATOR,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genRepeat),
                     nullptr,
                     nullptr);

    dict.addCodeWord("UNTIL", "FORTH",
                     ForthState::GENERATOR,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genUntil),
                     nullptr,
                     nullptr);

    dict.addCodeWord("LEAVE", "FORTH",
                     ForthState::GENERATOR,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genLeave),
                     nullptr,
                     nullptr);

    dict.addCodeWord("LOOP", "FORTH",
                     ForthState::GENERATOR,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genLoop),
                     nullptr,
                     nullptr);

    dict.addCodeWord("+LOOP", "FORTH",
                     ForthState::GENERATOR,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genPlusLoop),
                     nullptr,
                     nullptr);

    dict.addCodeWord("DO", "FORTH",
                     ForthState::GENERATOR,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genDo),
                     nullptr,
                     nullptr);

    dict.addCodeWord("I", "FORTH",
                     ForthState::GENERATOR,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genI),
                     nullptr,
                     nullptr);

    dict.addCodeWord("J", "FORTH",
                     ForthState::GENERATOR,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genJ),
                     nullptr,
                     nullptr);

    dict.addCodeWord("K", "FORTH",
                     ForthState::GENERATOR,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genK),
                     nullptr,
                     nullptr);

    dict.addCodeWord("RECURSE", "FORTH",
                     ForthState::GENERATOR,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genRecurse),
                     nullptr,
                     nullptr);

    dict.addCodeWord("REDO", "FORTH",
                     ForthState::GENERATOR,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genRedo),
                     nullptr,
                     nullptr);
}


// Floats on data stack
void moveTOSToXMM0() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    // Reserve a temporary memory location for the double (on the stack or a variable).
    assembler->sub(asmjit::x86::rsp, 8); // Make space on the stack (8 bytes for a double).
    // Store the value from RDI into the stack (temporary memory).
    assembler->mov(asmjit::x86::ptr(asmjit::x86::rsp), asmjit::x86::r13);
    // Load the value into xmm0 from memory.
    assembler->movsd(asmjit::x86::xmm0, asmjit::x86::ptr(asmjit::x86::rsp));
    // Clean up temporary memory (restore the stack pointer).
    assembler->add(asmjit::x86::rsp, 8);
}


static void genFDot() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    assembler->comment(" ; -- FDot");
    asmjit::x86::Gp tmpReg = asmjit::x86::rcx;
    popDS(tmpReg);
    assembler->movq(asmjit::x86::xmm0, tmpReg);
    assembler->push(asmjit::x86::rdi);
    assembler->call(asmjit::imm(reinterpret_cast<void *>(spit_number_f)));
    assembler->pop(asmjit::x86::rdi);
}

void compile_pushLiteralFloat(const double literal) {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    // Reserve space on the stack
    assembler->comment("; -- LITERAL float (make space for double)");
    compile_DUP();

    // Treat the double value as a raw 64-bit integer (`uint64_t`)
    uint64_t rawLiteral = *reinterpret_cast<const uint64_t *>(&literal);

    // Load the raw 64-bit literal into R13
    assembler->comment("; -- Load floating-point literal into R13");
    assembler->mov(asmjit::x86::r13, asmjit::imm(rawLiteral));

    assembler->commentf("; -- TOS is %f \n", literal);
}

static void genFPlus() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    asmjit::x86::Gp firstVal = asmjit::x86::rax;
    asmjit::x86::Gp secondVal = asmjit::x86::rbx;

    assembler->comment(" ; Add two floating point values from the stack");
    popDS(firstVal); // Pop the first floating point value
    popDS(secondVal); // Pop the second floating point value
    assembler->movq(asmjit::x86::xmm0, firstVal); // Move the first value to XMM0
    assembler->movq(asmjit::x86::xmm1, secondVal); // Move the second value to XMM1
    assembler->addsd(asmjit::x86::xmm0, asmjit::x86::xmm1); // Add the two floating point values
    assembler->movq(firstVal, asmjit::x86::xmm0); // Move the result back to a general-purpose register
    pushDS(firstVal); // Push the result back onto the stack
}

static void genFSub() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    asmjit::x86::Gp firstVal = asmjit::x86::rax;
    asmjit::x86::Gp secondVal = asmjit::x86::rbx;


    assembler->comment(" ; floating point subtraction");
    popDS(firstVal); // Pop the first floating point value
    popDS(secondVal); // Pop the second floating point value
    assembler->movq(asmjit::x86::xmm0, secondVal);
    assembler->movq(asmjit::x86::xmm1, firstVal);
    assembler->subsd(asmjit::x86::xmm0, asmjit::x86::xmm1); // Subtract the floating point values
    assembler->movq(firstVal, asmjit::x86::xmm0); // Move the result back to a general-purpose register
    pushDS(firstVal); // Push the result back onto the stack
}


static void genFMul() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    asmjit::x86::Gp firstVal = asmjit::x86::rax;
    asmjit::x86::Gp secondVal = asmjit::x86::rbx;

    assembler->comment(" ; Multiply two floating point values from the stack");
    popDS(firstVal); // Pop the first floating point value
    popDS(secondVal); // Pop the second floating point value
    assembler->movq(asmjit::x86::xmm0, firstVal); // Move the first value to XMM0
    assembler->movq(asmjit::x86::xmm1, secondVal); // Move the second value to XMM1
    assembler->mulsd(asmjit::x86::xmm0, asmjit::x86::xmm1); // Multiply the floating point values
    assembler->movq(firstVal, asmjit::x86::xmm0); // Move the result back to a general-purpose register
    pushDS(firstVal); // Push the result back onto the stack
}

static void genFDiv() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    asmjit::x86::Gp firstVal = asmjit::x86::rax;
    asmjit::x86::Gp secondVal = asmjit::x86::rbx;

    assembler->comment(" ; Divide two floating point values from the stack");
    popDS(firstVal); // Pop the first floating point value
    popDS(secondVal); // Pop the second floating point value
    assembler->movq(asmjit::x86::xmm0, secondVal);
    assembler->movq(asmjit::x86::xmm1, firstVal);
    assembler->divsd(asmjit::x86::xmm0, asmjit::x86::xmm1); // Divide the floating point values
    assembler->movq(firstVal, asmjit::x86::xmm0); // Move the result back to a general-purpose register
    pushDS(firstVal); // Push the result back onto the stack
}


static void genFMod() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    asmjit::x86::Gp firstVal = asmjit::x86::rax;
    asmjit::x86::Gp secondVal = asmjit::x86::rbx;

    assembler->comment(" ; Modulus two floating point values from the stack");
    popDS(firstVal); // Pop the first floating point value
    popDS(secondVal); // Pop the second floating point value
    assembler->movq(asmjit::x86::xmm0, secondVal);
    assembler->movq(asmjit::x86::xmm1, firstVal);
    assembler->divsd(asmjit::x86::xmm0, asmjit::x86::xmm1); // Divide the values
    assembler->roundsd(asmjit::x86::xmm0, asmjit::x86::xmm0, 1); // Floor the result
    assembler->mulsd(asmjit::x86::xmm0, asmjit::x86::xmm1); // Multiply back
    assembler->movq(firstVal, asmjit::x86::xmm0); // Move the intermediate result to firstVal
    assembler->movq(asmjit::x86::xmm0, secondVal); // Move the first value back to XMM0
    assembler->movq(asmjit::x86::xmm1, firstVal); // Move the intermediate result to XMM1
    assembler->subsd(asmjit::x86::xmm0, asmjit::x86::xmm1); // Subtract to get modulus
    assembler->movq(firstVal, asmjit::x86::xmm0); // Move the result back to a general-purpose register
    pushDS(firstVal); // Push the result back onto the stack
}


static void genFMax() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    asmjit::x86::Gp firstVal = asmjit::x86::rax;
    asmjit::x86::Gp secondVal = asmjit::x86::rbx;


    assembler->comment(" ; Find the maximum of two floating point values from the stack");
    popDS(firstVal); // Pop the first floating point value
    popDS(secondVal); // Pop the second floating point value
    assembler->movq(asmjit::x86::xmm0, firstVal); // Move the first value to XMM0
    assembler->movq(asmjit::x86::xmm1, secondVal); // Move the second value to XMM1
    assembler->maxsd(asmjit::x86::xmm0, asmjit::x86::xmm1); // Compute the maximum of the two values
    assembler->movq(firstVal, asmjit::x86::xmm0); // Move the result back to a general-purpose register
    pushDS(firstVal); // Push the result back onto the stack
}

static void genFMin() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    asmjit::x86::Gp firstVal = asmjit::x86::rax;
    asmjit::x86::Gp secondVal = asmjit::x86::rbx;


    assembler->comment(" ; Find the minimum of two floating point values from the stack");
    popDS(firstVal); // Pop the first floating point value
    popDS(secondVal); // Pop the second floating point value
    assembler->movq(asmjit::x86::xmm0, firstVal); // Move the first value to XMM0
    assembler->movq(asmjit::x86::xmm1, secondVal); // Move the second value to XMM1
    assembler->minsd(asmjit::x86::xmm0, asmjit::x86::xmm1); // Compute the minimum of the two values
    assembler->movq(firstVal, asmjit::x86::xmm0); // Move the result back to a general-purpose register
    pushDS(firstVal); // Push the result back onto the stack
}


static void genSin() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    asmjit::x86::Gp val = asmjit::x86::rax;
    assembler->comment(" ; Compute the sine of a floating point value from the stack");
    popDS(val); // Pop the floating point value from the stack
    assembler->movq(asmjit::x86::xmm0, val); // Move the value to XMM0

    // Call the sin() function
    assembler->sub(asmjit::x86::rsp, 8); // Reserve space on stack
    assembler->call(reinterpret_cast<void *>(static_cast<double(*)(double)>(sin)));
    assembler->add(asmjit::x86::rsp, 8); // Free reserved space

    assembler->movq(val, asmjit::x86::xmm0); // Move the result back to a general-purpose register
    pushDS(val); // Push the result back onto the stack
}

static void genCos() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    asmjit::x86::Gp val = asmjit::x86::rax;
    assembler->comment(" ; Compute the cos of a floating point value from the stack");
    popDS(val); // Pop the floating point value from the stack
    assembler->movq(asmjit::x86::xmm0, val); // Move the value to XMM0

    // Call the cos() function
    assembler->sub(asmjit::x86::rsp, 8); // Reserve space on stack
    assembler->call(reinterpret_cast<void *>(static_cast<double(*)(double)>(cos)));
    assembler->add(asmjit::x86::rsp, 8); // Free reserved space

    assembler->movq(val, asmjit::x86::xmm0); // Move the result back to a general-purpose register
    pushDS(val); // Push the result back onto the stack
}

static void genFAbs() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    asmjit::x86::Gp val = asmjit::x86::rax;
    asmjit::x86::Gp mask = asmjit::x86::rbx;
    uint64_t absMask = 0x7FFFFFFFFFFFFFFF; // Mask to clear the sign bit

    assembler->comment(" ; Compute the absolute value of a floating point value from the stack");
    popDS(val); // Pop the floating point value from the stack
    assembler->mov(mask, absMask); // Move the mask into a register
    assembler->and_(val, mask); // Clear the sign bit
    pushDS(val); // Push the result back onto the stack
}

static void genFLess() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    asmjit::x86::Gp firstVal = asmjit::x86::rax;
    asmjit::x86::Gp secondVal = asmjit::x86::rbx;

    assembler->comment(" ; Compare if second floating-point value is less than the first one");
    popDS(secondVal); // Pop the second floating-point value (firstVal should store the second one)
    popDS(firstVal); // Pop the first floating-point value (secondVal should store the first one)

    assembler->movq(asmjit::x86::xmm0, firstVal); // Move the second value to XMM0
    assembler->movq(asmjit::x86::xmm1, secondVal); // Move the first value to XMM1
    assembler->comisd(asmjit::x86::xmm0, asmjit::x86::xmm1); // Compare the two values

    assembler->setb(asmjit::x86::al); // Set AL to 1 if less than, 0 otherwise

    assembler->movzx(firstVal, asmjit::x86::al); // Zero extend AL to the full register

    // Now convert the boolean result to the expected -1 for true and 0 for false
    assembler->neg(firstVal); // Perform two's complement negation to get -1 if AL was set to 1

    pushDS(firstVal); // Push the result (-1 for true, 0 for false)
}

static void genFGreater() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    asmjit::x86::Gp firstVal = asmjit::x86::rax;
    asmjit::x86::Gp secondVal = asmjit::x86::rbx;


    assembler->comment(" ; Compare if second floating-point value is greater than the first one");
    popDS(firstVal); // Pop the first floating-point value
    popDS(secondVal); // Pop the second floating-point value
    assembler->movq(asmjit::x86::xmm0, firstVal); // Move the first value to XMM0
    assembler->movq(asmjit::x86::xmm1, secondVal); // Move the second value to XMM1
    assembler->comisd(asmjit::x86::xmm0, asmjit::x86::xmm1); // Compare the two values

    assembler->setb(asmjit::x86::al); // Set AL to 1 if less than, 0 otherwise

    assembler->movzx(firstVal, asmjit::x86::al); // Zero extend AL to the full register

    // Now convert the boolean result to the expected -1 for true and 0 for false
    assembler->neg(firstVal); // Perform two's complement negation to get -1 if AL was set to 1

    pushDS(firstVal); // Push the result (-1 for true, 0 for false)
}

static void genIntToFloat() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    asmjit::x86::Gp intVal = asmjit::x86::rax;
    assembler->comment(" ; Convert integer to floating point");
    popDS(intVal); // Pop integer value from the stack
    assembler->cvtsi2sd(asmjit::x86::xmm0, intVal); // Convert integer in RAX to double in XMM0
    assembler->movq(intVal, asmjit::x86::xmm0); // Move the double from XMM0 back to a general-purpose register
    pushDS(intVal); // Push the floating point value back onto the stack
}


static void genFloatToInt() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    asmjit::x86::Gp floatVal = asmjit::x86::rax;

    assembler->comment(" ; Convert floating point to integer");
    popDS(floatVal); // Pop floating point value from the stack
    assembler->movq(asmjit::x86::xmm0, floatVal); // Move the floating point value to XMM0
    assembler->cvttsd2si(floatVal, asmjit::x86::xmm0); // Convert floating point in XMM0 to integer in RAX
    pushDS(floatVal); // Push the integer value back onto the stack
}


static void genFloatToIntRounding() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    asmjit::x86::Gp floatVal = asmjit::x86::rax;

    assembler->comment(" ; Convert floating point to integer with rounding");
    popDS(floatVal); // Pop floating point value from the stack
    assembler->movq(asmjit::x86::xmm0, floatVal); // Move the floating point value to XMM0
    assembler->roundsd(asmjit::x86::xmm0, asmjit::x86::xmm0, 0b00); // Round to nearest
    assembler->cvtsd2si(floatVal, asmjit::x86::xmm0); // Convert rounded floating point to integer in RAX
    pushDS(floatVal); // Push the integer value back onto the stack
}

static void genFloatToIntFloor() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    asmjit::x86::Gp floatVal = asmjit::x86::rax;

    assembler->comment(" ; Convert floating point to integer using floor");
    popDS(floatVal); // Pop floating point value from the stack
    assembler->movq(asmjit::x86::xmm0, floatVal); // Move the floating point value to XMM0
    assembler->roundsd(asmjit::x86::xmm0, asmjit::x86::xmm0, 0b01); // Round down (floor) to nearest integer
    assembler->cvtsd2si(floatVal, asmjit::x86::xmm0); // Convert to integer
    pushDS(floatVal); // Push the integer result back onto the stack
}

static void genSqrt() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    asmjit::x86::Gp val = asmjit::x86::rax;
    assembler->comment(" ; Compute the square root of a floating point value from the stack");
    popDS(val); // Pop the floating point value from the stack
    assembler->movq(asmjit::x86::xmm0, val); // Move the value to XMM0

    // Call the sqrt() function
    assembler->sub(asmjit::x86::rsp, 8); // Reserve space on the stack
    assembler->call(reinterpret_cast<void *>(static_cast<double(*)(double)>(sqrt)));
    assembler->add(asmjit::x86::rsp, 8); // Free reserved space

    assembler->movq(val, asmjit::x86::xmm0); // Move the result back to a general-purpose register
    pushDS(val); // Push the result back onto the stack
}


constexpr static double tolerance = 1e-7;

// exactly equivalent to : f= f- fabs 1e-7 f< ;
static void genFEquals() {
    genFSub();
    genFAbs();
    compile_pushLiteralFloat(tolerance);
    genFLess();
}


void code_generator_add_float_words() {
    ForthDictionary &dict = ForthDictionary::instance();


    dict.addCodeWord("f=", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genFEquals),
                     code_generator_build_forth(genFEquals),
                     nullptr
    );

    dict.addCodeWord("fsqrt", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genSqrt),
                     code_generator_build_forth(genSqrt),
                     nullptr
    );

    dict.addCodeWord("floor", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genFloatToIntFloor),
                     code_generator_build_forth(genFloatToIntFloor),
                     nullptr
    );

    dict.addCodeWord("fround", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genFloatToIntRounding),
                     code_generator_build_forth(genFloatToIntRounding),
                     nullptr
    );


    dict.addCodeWord("ftruncate", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genFloatToInt),
                     code_generator_build_forth(genFloatToInt),
                     nullptr
    );

    dict.addCodeWord("f>s", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genFloatToInt),
                     code_generator_build_forth(genFloatToInt),
                     nullptr
    );

    dict.addCodeWord("s>f", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genIntToFloat),
                     code_generator_build_forth(genIntToFloat),
                     nullptr
    );


    dict.addCodeWord("f>", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genFGreater),
                     code_generator_build_forth(genFGreater),
                     nullptr
    );


    dict.addCodeWord("f<", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genFLess),
                     code_generator_build_forth(genFLess),
                     nullptr
    );


    dict.addCodeWord("sin", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genSin),
                     code_generator_build_forth(genSin),
                     nullptr
    );

    dict.addCodeWord("cos", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genCos),
                     code_generator_build_forth(genCos),
                     nullptr
    );


    dict.addCodeWord("fabs", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genFAbs),
                     code_generator_build_forth(genFAbs),
                     nullptr
    );


    dict.addCodeWord("fmin", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genFMin),
                     code_generator_build_forth(genFMin),
                     nullptr
    );


    dict.addCodeWord("fmax", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genFMax),
                     code_generator_build_forth(genFMax),
                     nullptr
    );


    dict.addCodeWord("fmod", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genFMod),
                     code_generator_build_forth(genFMod),
                     nullptr
    );


    dict.addCodeWord("f/", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genFDiv),
                     code_generator_build_forth(genFDiv),
                     nullptr
    );


    dict.addCodeWord("f*", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genFMul),
                     code_generator_build_forth(genFMul),
                     nullptr
    );


    dict.addCodeWord("f-", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genFSub),
                     code_generator_build_forth(genFSub),
                     nullptr
    );


    dict.addCodeWord("f.", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genFDot),
                     code_generator_build_forth(genFDot),
                     nullptr
    );

    dict.addCodeWord("f+", "FORTH",
                     ForthState::EXECUTABLE,
                     ForthWordType::WORD,
                     static_cast<ForthFunction>(&genFPlus),
                     code_generator_build_forth(genFPlus),
                     nullptr
    );
}
