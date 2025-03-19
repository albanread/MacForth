#ifndef LETCODEGENERATOR_H
#define LETCODEGENERATOR_H

#include <string>
#include <iostream>
#include <unordered_map>
#include <map>
#include "ParseLet.h" // Include your AST definitions here
#include "Singleton.h" // Include the Singleton template class
#include <sstream>
#include "RegisterTracker.h"


/**
 * LetCodeGenerator Responsibilities:
 * ----------------------------------
 * 1. Request Resources:
 *    - Asks the RegisterTracker to allocate and free registers as needed.
 *    - Allocates registers tied to specific expressions (using unique names).
 *
 * 2. Emit Code:
 *    - Emits assembly instructions for expressions using asmjit.
 *    - Handles intermediate computations and writes output to registers.
 *
 * 3. Manage Temporary Registers:
 *    - Handles allocation and freeing of temporary registers (e.g., for constants like 0.0).
 *    - Ensures all registers are released once their purpose is served.
 *
 * 4. Debugging and Comments:
 *    - Adds human-readable comments in the generated code for debugging purposes.
 *    - Provides clear tracking of register usage.
 *
 * 5. Error Handling:
 *    - Ensures invalid inputs (e.g., null pointers, unknown operators) are caught and handled gracefully.
 *
 * Responsibilities the LetCodeGenerator does *NOT* handle:
 * -------------------------------------------------------
 * 1. Managing the Register Pool:
 *    - It does not decide the total number of registers or handle register spilling.
 *      These are handled by the RegisterTracker.
 * 2. Resource Lifetimes Beyond the Current Scope:
 *    - Register cleanup is the generator’s job for the expressions it processes.
 *
 * In short, LetCodeGenerator ensures that registers are allocated, used, and freed
 * during code generation, while delegating tracking and management tasks to the
 * RegisterTracker.
 */


class LetCodeGenerator : public Singleton<LetCodeGenerator> {
    friend class Singleton<LetCodeGenerator>; // Allow Singleton to construct LetCodeGenerator instances

public:
    // Public methods
    void initialize();
    void generateCode(const ASTNode *node);

    void saveGPcache(asmjit::x86::Assembler *assembler);

    void restoreGPCache(asmjit::x86::Assembler *assembler);

    /// Emit code for setting up the spill slot base
    static void setupSpillSlotBase();

    /// Emit the prologue for Let expression evaluation
    static void emitFunctionPrologue();

private:
    // Private constructor
    LetCodeGenerator();

    // Generate Let statement code
    void generateLetStatement(const LetStatement *letStmt);

    // Generate WHERE clause code
    void generateWhereClause(const WhereClause *wc);

    // Generate expression code
    void generateExpression(Expression *expr);
    void generateLiteralExpr(const Expression *expr);
    void generateVariableExpr(const Expression *expr);

    void commentOnExpression(const Expression *expr);

    void generateFunctionExpr(const Expression *expr);
    void generateBinaryOpExpr(Expression *expr);
    void generateUnaryOpExpr(const Expression *expr);

    void emitBinaryOperation(const std::string &op,
                             const asmjit::x86::Xmm &exprReg,
                             const asmjit::x86::Xmm &lhsReg,
                             const asmjit::x86::Xmm &rhsReg,
                             [[maybe_unused]] Expression *lhsExpr,
                             [[maybe_unused]] Expression *rhsExpr);

    void callMathFunction(const std::string &funcName, const asmjit::x86::Xmm &arg1Reg,
                          const asmjit::x86::Xmm &arg2Reg = asmjit::x86::Xmm());

    void emitExponentiation(asmjit::x86::Xmm exprReg,
                            asmjit::x86::Xmm lhsReg,
                            asmjit::x86::Xmm rhsReg);

    void emitLoadDoubleLiteral(const std::string &literalString, asmjit::x86::Xmm destReg);

    bool isConstantExpression(Expression *expr);


    void printRegisterUsage() const;

    static void preserveAndCallFunction(void *func);

    std::string expressionToString(const Expression *expr);

    std::string getUniqueTempName(const Expression *expr);

    std::string expressionToText(const Expression *expr);

    void loadArguments(const std::vector<std::string> &params);


    /// Stores variables generated in Let expressions
    std::unordered_map<std::string, std::string> variables;
    std::map<std::string, double> globalVariables;
    std::unordered_map<const Expression*, std::string> expressionNameMap;
    /// Register tracker reference
    RegisterTracker &tracker = RegisterTracker::instance();
    bool debug = false;
};

#endif // LETCODEGENERATOR_H