#include <cstdint>
#include <ForthDictionaryEntry.h>
#include <JitContext.h>
#include "FlowLabels.h"




[[maybe_unused]] static void pushDS(const asmjit::x86::Gp& reg);
[[maybe_unused]] static void popDS(const asmjit::x86::Gp& reg);
[[maybe_unused]] static void pushRS(const asmjit::x86::Gp& reg);
[[maybe_unused]] static void popRS(const asmjit::x86::Gp& reg);

bool initialize_assembler(asmjit::x86::Assembler *&assembler);

void code_generator_initialize();

ForthFunction code_generator_build_forth(ForthFunction fn);

void code_generator_startFunction(const std::string &name);

void compile_return();

ForthFunction code_generator_finalizeFunction(const std::string &name);

void code_generator_reset();

void compile_pushLiteral(const int64_t literal);

void compile_pushLiteralFloat(const double literal);

void compile_call_C(void (*func)());

void compile_call_forth(void (*func)(), const std::string &forth_word);

void compile_call_C_char(void (*func)(char*));

void stack_self();

void code_generator_puts_no_crlf(const char *str);



void code_generator_add_immediate_words();

void code_generator_add_operator_words();

void code_generator_add_stack_words();

void code_generator_add_io_words();

void code_generator_add_control_flow_words();

void code_generator_add_vocab_words() ;

void code_generator_add_float_words() ;

void cpush(int64_t value);

void cfpush(double value);

double cfpop();

[[maybe_unused]]  static void genFetch(uint64_t address);

int64_t cpop();