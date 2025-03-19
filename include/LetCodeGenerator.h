#ifndef LET_CODE_GENERATOR_H
#define LET_CODE_GENERATOR_H

#include <asmjit/asmjit.h>
#include <iostream>
#include <unordered_map>
#include <string>
#include <map>
#include "ParseLet.h" // AST definitions: LetStatement, Expression, etc.


// Fixed-size static global memory pool (per thread)
static constexpr size_t GLOBAL_MEMORY_SIZE = 64 * 1024; // 64 KB memory pool
inline thread_local uint8_t gGlobalStaticMemory[GLOBAL_MEMORY_SIZE];


class LetCodeGenerator {
public:
    LetCodeGenerator() : globalMemory(nullptr) {
    }

    // Prepare code generation: allocate or free
    void allocateGlobalMemory(const std::vector<std::string> &variables);

    void freeGlobalMemory();

    void printRegisterMaps();

    asmjit::x86::Xmm getRegister(const std::string &tmpName, Expression *expr);

    void addCompactRegisterComments();

    void addRegisterComments();

    asmjit::x86::Xmm reloadSpilledVariable(const std::string &varName);


    void spillVariable(const std::string &var, asmjit::x86::Xmm reg);

    asmjit::x86::Xmm reloadRegister(const std::string &var, size_t spillOffset);

    void clearMappings(const std::string &var, asmjit::x86::Xmm reg);

    void emitRegisterSave();

    void emitRegisterRestore();

    void emitSpillAndReloadForArguments(const std::vector<Expression *> &args);

    bool isVariableSpilled(const std::string &varName);

    asmjit::x86::Xmm findRegisterForVariable(const std::string &varName);

    std::string findVariableForRegister(const asmjit::x86::Xmm &xmmReg);

    asmjit::x86::Xmm findOrAllocateTempRegister(const std::string &tmpName, Expression *expr,
                                                const std::string &varName);

    void copyVariableToTempRegister(const std::string &varName, const std::string &tmpName, asmjit::x86::Xmm varReg,
                                    asmjit::x86::Xmm tmpReg);

    asmjit::x86::Xmm emitChildExpression(Expression *childExpr);

    void emitBinaryOperation(const std::string &op, const asmjit::x86::Xmm &exprReg, const asmjit::x86::Xmm &lhsReg,
                             const asmjit::x86::Xmm &rhsReg, Expression *lhsExpr, Expression *rhsExpr);

    void decrementUsage(const std::string &var);

    void emitExponentiation(asmjit::x86::Xmm exprReg, asmjit::x86::Xmm lhsReg, asmjit::x86::Xmm rhsReg);

    void logJitOperation(const std::string &message);

    void decrementUsageByRegister(asmjit::x86::Xmm reg);

    std::string expressionToString(Expression *expr);

    asmjit::x86::Xmm resolveTempRegister(const std::string &tmpName);

    // Main entry: build native code from AST
    void generate(const LetStatement &root);

private:

    bool preloading =true;
    // XMM register usage
    std::unordered_map<std::string, asmjit::x86::Xmm> registerMap; // map "varName" -> Xmm
    std::unordered_map<uint32_t, std::string> reverseRegisterMap; // optional: Xmm.id() -> name
    // int xmmIndex;
    std::unordered_map<Expression *, std::string> tempNameMap;
    size_t spillOffsetCounter = 0; // next free offset in spill memory
    std::unordered_map<std::string, size_t> spillSlots; // var -> spill offset
    // For memory-based variable storage (if needed)
    uint8_t *globalMemory;
    size_t gTempCounter = 0;
    const u_int32_t spill_reserve = 16 * 16;
    // size_t memorySize = size_t(GLOBAL_MEMORY_SIZE);
    std::unordered_map<std::string, size_t> varOffsets;
    std::unordered_map<std::string, size_t> usageTracker;
    std::unordered_map<std::string, asmjit::x86::Xmm> literalCache;
    std::set<std::string> inProgressExpressions;

    struct TempRegInfo {
        asmjit::x86::Xmm reg; // The assigned register
        std::string spillSlot; // Spill location (if spilled)
        bool isSpilled = false; // Whether it's currently spilled
    };

    std::map<std::string, TempRegInfo> tempRegisters; // Tracks _tmpN lifecycle


    // Utility: allocate an XMM register for a given name
    asmjit::x86::Xmm allocateRegister(const std::string &var);

    asmjit::x86::Xmm findFreeRegister();

    bool isConstantValue(const std::string &varName) const;

    asmjit::x86::Xmm spillRegister(const std::string &var);

    // If we need to free registers
    void freeRegister(const std::string &varOrLiteral);

    // Preallocate registers for all relevant variables/expressions
    void preallocateRegisters(const LetStatement &root);

    void eraseVariablesFromExpression(Expression *expr);

    void decrementVariablesFromExpression(Expression *expr);

    // Recursively collect variables from an expression
    void collectVariablesFromExpression(Expression *expr);

    // Emit top-level code for an expression node
    void emitExpression(Expression *expr);

    bool isConstantExpression(Expression *expr);

    // Emit code for a WHERE clause
    void emitWhereClause(WhereClause *whereClause);

    // A small helper to ensure each expression node gets a unique name
    std::string getUniqueTempName(Expression *expr);

    void saveCallerSavedRegisters();

    void restoreCallerSavedRegisters();

    void callMathFunction(const std::string &funcName, const asmjit::x86::Xmm &arg1Reg,
                          const asmjit::x86::Xmm &arg2Reg);

    // For referencing or storing literal data
    void emitLoadDoubleLiteral(const std::string &literalString, asmjit::x86::Xmm destReg);

    void zeroUsage(const std::string &var);

    void incrementUsage(const std::string &var);
};

#endif // LET_CODE_GENERATOR_H
