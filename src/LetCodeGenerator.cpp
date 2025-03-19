#include "LetCodeGenerator.h"
#include <cmath>
#include <sstream>

#include "CodeGenerator.h"
#include "Settings.h"

// Utility function to check if a substring exists within a string
bool contains(const std::string &str, const std::string &substring) {
    return str.find(substring) != std::string::npos;
}

inline double fake_pow(double base, double exponent) {
    std::cerr << "fake_pow(" << base << ", " << exponent << ")" << std::endl;
    if (exponent == 2.0) {
        return base * base;
    }
    // Extend for other cases if needed
    return 1.0; // Placeholder for unsupported exponents
}

inline double display(double n) {
    std::cerr << n << std::endl;
    return n;
}

using FunctionPtr = double(*)(double);
std::unordered_map<std::string, FunctionPtr> SingleFuncMap = {
    {"sin", &sin},
    {"cos", &cos},
    {"tan", &tan},
    {"exp", &exp},
    {"log", &log},
    {"ln", &log}, // alias for natural logarithm
    {"fabs", &fabs},
    {"abs", &fabs}, // alias for fabs (floating-point absolute value)
    {"sinh", &sinh},
    {"cosh", &cosh},
    {"tanh", &tanh},
    {"asin", &asin},
    {"acos", &acos},
    {"atan", &atan},
    {"log2", &log2},
    {"log10", &log10},
    {"display", &display},
};

using DualFunctionPtr = double(*)(double, double);
std::unordered_map<std::string, DualFunctionPtr> DualFuncMap = {
    {"atan2", &atan2},
    {"pow", &pow},
    {"hypot", &hypot},
    {"fmod", &fmod},
    {"remainder", &remainder},
    {"fmin", &fmin},
    {"fmax", &fmax},
};


// -----------------------------------------------------------------------------
// generate() : Entry Point
// -----------------------------------------------------------------------------
void LetCodeGenerator::generate(const LetStatement &root) {
    //    if (jitLogging) std::cerr << "Function name generate" << std::endl;

    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    gTempCounter = 0;
    spillOffsetCounter = 0;
    registerMap.clear();
    reverseRegisterMap.clear();
    spillSlots.clear();
    varOffsets.clear();
    tempNameMap.clear();
    literalCache.clear();
    inProgressExpressions.clear();

    assembler->push(asmjit::x86::rdi); // we clobber this
    assembler->commentf("Global memory address: %p to RDI", gGlobalStaticMemory);
    assembler->mov(asmjit::x86::rdi, asmjit::imm(reinterpret_cast<uint64_t>(gGlobalStaticMemory)));


    preloading = true;

    // ----------------------------------------------------------------------------
    // Step 2: Forth "prologue" - pop inputs from stack into registers in reverse
    // ----------------------------------------------------------------------------
    assembler->comment("; === Load Variable(s) ===");
    for (auto it = root.inputParams.rbegin(); it != root.inputParams.rend(); ++it) {
        // Reverse iteration
        const auto &inputVar = *it; // Access variable
        asmjit::x86::Xmm reg = allocateRegister(inputVar);

        assembler->commentf("; Load variable from FORTH stack: %s", inputVar.c_str());

        assembler->movq(reg, asmjit::x86::r13);
        // drop
        assembler->comment("; -- DROP ");
        assembler->mov(asmjit::x86::r13, asmjit::x86::r12); // Move TOS-1 into TOS
        assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15)); // Move TOS-2 into TOS-1
        assembler->add(asmjit::x86::r15, 8); // Adjust stack pointer
    }


    preloading = false;
    preallocateRegisters(root);

    // ----------------------------------------------------------------------------
    // Step 3: Emit WHERE clauses (each is effectively varName = some_expr)
    // ----------------------------------------------------------------------------
    assembler->comment("; === Evaluate Where clause(s) ===");

    for (const auto &whereClause: root.whereClauses) {
        emitWhereClause(whereClause.get());
    }


    assembler->comment("; === Evaluate Main Expression(s) ===");

    // ----------------------------------------------------------------------------
    // Step 4: Emit final expressions (in reverse for forth stack)
    // ----------------------------------------------------------------------------

    size_t numExprs = root.expressions.size();
    for (size_t i = numExprs; i-- > 0;) {
        Expression *expr = root.expressions[i].get();
        std::string outVar = root.outputVars[i]; // Output variable corresponding to this expression
        assembler->commentf("; Emit expression %zu for output var '%s'", i, outVar.c_str());
        emitExpression(expr);
        // The result of this expression is in a unique temporary register
        auto name = getUniqueTempName(expr);
        asmjit::x86::Xmm exprReg = getRegister(name, expr);
        assembler->commentf("; Pushing result of '%s' onto stack", outVar.c_str());
        assembler->sub(asmjit::x86::r15, 8); // Allocate space on the stack
        assembler->mov(asmjit::x86::ptr(asmjit::x86::r15), asmjit::x86::r12);
        assembler->mov(asmjit::x86::r12, asmjit::x86::r13);
        assembler->movq(asmjit::x86::r13, exprReg);
        decrementUsage(name);
    }
    assembler->comment("; === End of Main Expression(s) ===");
    assembler->pop(asmjit::x86::rdi); // Restore RDI that we clobber

    addRegisterComments();
}


// -----------------------------------------------------------------------------
// Preallocate registers for variables in WHERE and for each expression node
// -----------------------------------------------------------------------------
void LetCodeGenerator::preallocateRegisters(const LetStatement &root) {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    for (auto &v: root.inputParams) {
        allocateRegister(v);
    }

    //for (auto &v: root.outputVars) {
    //    allocateRegister(v);
    //}

    for (auto &wc: root.whereClauses) {
        allocateRegister(wc->varName);
    }

    for (auto &wc: root.expressions) {
        allocateRegister(wc->value);
    }


    addRegisterComments();
}

void LetCodeGenerator::eraseVariablesFromExpression(Expression *expr) {
    if (!expr) return;

    std::string tempName = getUniqueTempName(expr);
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    usageTracker[tempName] = 1;
    decrementUsage(tempName);

    assembler->commentf("; erasing usage of %s", tempName.c_str());
    for (auto &child: expr->children) {
        eraseVariablesFromExpression(child.get());
    }
}


// -----------------------------------------------------------------------------
// Collect variables or sub-expressions needing registers
// -----------------------------------------------------------------------------
void LetCodeGenerator::collectVariablesFromExpression(Expression *expr) {
    //    if (jitLogging) std::cerr << "collectVariablesFromExpression - collects variables" << std::endl;
    if (!expr) return;
    std::string tempName = getUniqueTempName(expr);
    // If it's a variable reference, allocate a register for it
    if (expr->type == ExprType::VARIABLE) {
        getRegister(expr->value, expr);
    }
    // If it's a literal it has no real existence.
    if (expr->type == ExprType::LITERAL) {
        //getRegister(expr->value, expr);
    }

    getUniqueTempName(expr); // forces creation of a unique "temp name"

    for (auto &child: expr->children) {
        collectVariablesFromExpression(child.get());
    }

    if (registerMap.find(tempName) == registerMap.end()) {
        // Assign a new register if one is not already assigned.
        registerMap[tempName] = allocateRegister(expr->value);;
    }
}

// -----------------------------------------------------------------------------
// Unique temp name generator
// -----------------------------------------------------------------------------


std::string LetCodeGenerator::getUniqueTempName(Expression *expr) {
    // We'll store the pointer address as a key in a map, so each node gets exactly one name

    //    if (jitLogging) std::cerr << "Function getUniqueTempName - gets unique temp name" << std::endl;
    auto it = tempNameMap.find(expr);

    if (it != tempNameMap.end()) {
        //if (jitLogging) {
        //   std::cerr << "found temp name in map: " << it->second << std::endl;
        // }
        return it->second;
    }

    //    if (jitLogging) std::cerr << "Generating name" << std::endl;
    std::size_t hashCode = std::hash<Expression *>{}(expr); // Hash the pointer
    // Generate a new name for this node
    std::string prefix = "";
    if (expr->type == ExprType::VARIABLE || expr->type == ExprType::CONSTANT) {
        if (expr->isConstant) {
            prefix = "CST_" + expr->value;
        } else {
            prefix = "VAR_" + expr->value;
        }
    } else if (expr->type == ExprType::LITERAL) {
        prefix = "LIT_" + expr->value;
    } else if (expr->type == ExprType::FUNCTION) {
        prefix = "FNC_" + expr->value;
    } else if (expr->type == ExprType::BINARY_OP) {
        prefix = "BOP_" + expr->value;
    }

    if (prefix.empty()) prefix = "tmp";
    std::string newName = "_" + prefix + "_[" + std::to_string(++gTempCounter) + "]_" + std::to_string(
                              hashCode % 1000000);
    tempNameMap[expr] = newName;
    // Then allocate a register for it
    //    if (jitLogging) std::cerr << "generating unique temp name " << newName << std::endl;
    allocateRegister(newName);
    return newName;
}

void LetCodeGenerator::saveCallerSavedRegisters() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;
    assembler->comment("; === Saving caller-saved registers ===");

    // Iterate over XMM registers (xmm0 to xmm7)
    for (int regId = 0; regId <= 7; ++regId) {
        // Check if the register is associated with a variable in reverseRegisterMap
        auto it = reverseRegisterMap.find(regId);
        if (it == reverseRegisterMap.end()) {
            continue; // Skip unused registers
        }

        const std::string &varName = it->second; // Get the variable name

        // Delegate the spilling process to spillVariable
        // Ensure constants are only spilled once within spillVariable itself
        spillVariable(varName, asmjit::x86::xmm(regId));
    }
}

void LetCodeGenerator::restoreCallerSavedRegisters() {
}


void LetCodeGenerator::callMathFunction(const std::string &funcName, const asmjit::x86::Xmm &arg1Reg,
                                        const asmjit::x86::Xmm &arg2Reg = asmjit::x86::Xmm()) {
    //    if (jitLogging) std::cerr << "callMathFunction - calls math function" << std::endl;
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    assembler->commentf("; ====== call to C math: %s", funcName.c_str());

    // Try single-argument functions first
    auto singleIt = SingleFuncMap.find(funcName);


    if (singleIt != SingleFuncMap.end()) {
        // Single-argument function found

        // if arg1reg != xmm0
        if (arg1Reg.id() != 0) {
            assembler->movaps(asmjit::x86::xmm0, arg1Reg); // Move the floating-point argument to xmm0
        }

        assembler->push(asmjit::x86::rdi);
        assembler->call(asmjit::imm(reinterpret_cast<void *>(singleIt->second))); // Call the function
        assembler->pop(asmjit::x86::rdi);

        // repo
        //    if (jitLogging) std::cerr << "erasing: " << arg1Reg.id() << std::endl;
        reverseRegisterMap.erase(arg1Reg.id());
        //    if (jitLogging) std::cerr << "erasing: " << arg1Reg.id() << std::endl;
        reverseRegisterMap.erase(arg2Reg.id());

        return;
    }

    // Try dual-argument functions next
    auto dualIt = DualFuncMap.find(funcName);

    if (dualIt != DualFuncMap.end()) {
        if (!arg2Reg.isValid()) {
            std::cerr << "Dual-argument function requires two arguments but only one provided: " << funcName <<
                    std::endl;
            SignalHandler::instance().raise(22);
            return;
        }

        if (arg1Reg.id() != 0) {
            assembler->movaps(asmjit::x86::xmm0, arg1Reg); // arg1 -> xmm0
        }

        if (arg2Reg.id() != 1) {
            assembler->movaps(asmjit::x86::xmm1, arg2Reg); // arg2 -> xmm1
        }

        assembler->push(asmjit::x86::rdi);
        assembler->call(asmjit::imm(reinterpret_cast<void *>(dualIt->second))); // Call the function
        assembler->pop(asmjit::x86::rdi);

        return;
    }

    // If no function is found, report an error
    std::cerr << "Unknown function: " << funcName << std::endl;
    SignalHandler::instance().raise(22);
}

// -----------------------------------------------------------------------------
// Emit code for an expression node, storing the final result in the node's
// unique temp register (from getUniqueTempName(expr)).
// -----------------------------------------------------------------------------


void LetCodeGenerator::emitExpression(Expression *expr) {
    if (!expr) return;

    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler); // Initialize the assembler


    for (auto &kids: expr->children) {
        collectVariablesFromExpression(kids.get());
    }

    // STEP 1: Handle constant expressions first
    if (isConstantExpression(expr)) {
        //if (expr->isConstant) expr->processed = true;
        assembler->commentf("; Emitting constant expression: %s", expr->value.c_str());


        std::string tmpName = getUniqueTempName(expr);

        // Prevent infinite loops by checking in-progress expressions
        if (inProgressExpressions.count(tmpName) > 0) {
            assembler->commentf("; Skipping already in-progress constant expression: %s", tmpName.c_str());
            return;
        }

        auto it = literalCache.find(tmpName);
        if (it != literalCache.end()) {
            assembler->commentf("; Using cached constant for expression: %s", tmpName.c_str());
            return;
        }

        inProgressExpressions.insert(tmpName);
        assembler->commentf("; Computing and caching constant expression: %s", tmpName.c_str());

        // Emit code for any subexpressions of the constant
        if (expr->type == ExprType::BINARY_OP) {
            // Step 1: Emit left-hand side and get its register
            Expression *lhs = expr->children[0].get();
            //if (expr->isConstant) expr->processed = true;
            asmjit::x86::Xmm lhsReg = emitChildExpression(lhs);

            // Step 2: Emit right-hand side and get its register
            Expression *rhs = expr->children[1].get();
            //if (expr->isConstant) expr->processed = true;
            asmjit::x86::Xmm rhsReg = emitChildExpression(rhs);

            // Step 3: Allocate result register for the current expression
            std::string exprTmpName = getUniqueTempName(expr);
            asmjit::x86::Xmm exprReg = getRegister(exprTmpName, expr);
            incrementUsage(exprTmpName);
            // Only copy lhs to exprReg if they're different
            if (exprReg.id() != lhsReg.id()) {
                assembler->movaps(exprReg, lhsReg);
            }
            if (lhs->isConstant && rhs->isConstant) {
                assembler->commentf("; That was constant");
                expr->processed = true;
                auto varName = findVariableForRegister(lhsReg);
                decrementUsage(varName);
            }
            // Step 4: Emit the operator-specific assembly
            emitBinaryOperation(expr->value, exprReg, lhsReg, rhsReg, lhs, rhs);
            for (auto &kids: expr->children) {
                eraseVariablesFromExpression(kids.get());
            }

            return;
        }

        // Step 6: Simple constants (not binary) go directly here
        // the literal value may already be in a register?

        if (literalCache.count(expr->value) > 0) {
            assembler->commentf("; Using cached constant for expression: %s", expr->value.c_str());
            literalCache[tmpName] = literalCache[expr->value];
            inProgressExpressions.erase(tmpName); // Mark done
            assembler->commentf("; Done with const expression: %s", expressionToString(expr).c_str());
            for (auto &kids: expr->children) {
                eraseVariablesFromExpression(kids.get());
            }

            return;
        }

        asmjit::x86::Xmm reg = allocateRegister(tmpName);
        incrementUsage(tmpName);

        assembler->commentf("; Allocating register and emitting constant value: %s", tmpName.c_str());
        emitLoadDoubleLiteral(expr->value, reg);

        literalCache[tmpName] = reg;
        inProgressExpressions.erase(tmpName); // Mark done
        assembler->commentf("; Done with const expression: %s", expressionToString(expr).c_str());
        return;
    }

    switch (expr->type) {
        // --------------------------------------------------
        case ExprType::LITERAL:
        case ExprType::CONSTANT:
        case ExprType::VARIABLE: {
            std::string varName = expr->value;

            asmjit::x86::Xmm varReg;

            // Step 1: Check if the value is currently spilled
            if (isVariableSpilled(varName)) {
                // The variable is spilled; reload it
                std::cerr << "Reloading spilled variable: " << varName << std::endl;
                varReg = reloadSpilledVariable(varName); // Encapsulates spill handling
            } else {
                // Check if the variable is assigned to a register
                varReg = findRegisterForVariable(varName);
            }

            auto name = getUniqueTempName(expr);
            registerMap[name] = varReg;
            incrementUsage(name);

            if (expr->isConstant) expr->processed = true;


            break;
        }

        // --------------------------------------------------
        case ExprType::UNARY_OP: {
            //if (expr->processed) return;
            //if (expr->isConstant) expr->processed = true;
            Expression *child = expr->children[0].get();

            emitExpression(child);
            std::string childTmpName = getUniqueTempName(child);
            asmjit::x86::Xmm childReg = getRegister(childTmpName, child);
            std::string exprTmpName = getUniqueTempName(expr);
            asmjit::x86::Xmm exprReg = getRegister(exprTmpName, expr);

            if (expr->value == "neg") {
                assembler->comment("; Unary negation");
                // Move the child result into exprReg, then flip the sign

                if (exprReg.id() != childReg.id()) {
                    assembler->movaps(exprReg, childReg);
                }

                // Load 0.0 into a temp
                asmjit::x86::Xmm zeroReg = allocateRegister("_zero");
                emitLoadDoubleLiteral("0.0", zeroReg);

                assembler->subsd(exprReg, exprReg); // exprReg = exprReg - exprReg => 0
                assembler->subsd(exprReg, childReg); // exprReg = 0 - child => -child
            } else {
                std::cerr << ("Unknown unary operator: " + expr->value) << std::endl;
                SignalHandler::instance().raise(22);
            }
            decrementUsage("_zero");
            // addCompactRegisterComments
            break;
        }

        // --------------------------------------------------
        case ExprType::BINARY_OP: {
            if (expr->processed) return;

            // Step 1: Emit left-hand side and get its register
            Expression *lhs = expr->children[0].get();
            asmjit::x86::Xmm lhsReg = emitChildExpression(lhs);

            // Step 2: Emit right-hand side and get its register
            Expression *rhs = expr->children[1].get();
            asmjit::x86::Xmm rhsReg = emitChildExpression(rhs);

            // Step 3: Allocate result register for the current expression
            std::string exprTmpName = getUniqueTempName(expr);
            asmjit::x86::Xmm exprReg = getRegister(exprTmpName, expr);

            // Only copy lhs to exprReg if they're different
            if (exprReg.id() != lhsReg.id()) {
                assembler->movaps(exprReg, lhsReg);
            }
            // Step 4: Emit the operator-specific assembly
            emitBinaryOperation(expr->value, exprReg, lhsReg, rhsReg, lhs, rhs);

            if (lhs->isConstant && rhs->isConstant) {
                assembler->commentf("; That was constant");
                expr->processed = true;
                // dump this now.
                auto varName = findVariableForRegister(lhsReg);
                decrementUsage(varName);
            }
            // addCompactRegisterComments


            break;
        }


            assembler->commentf("; Emitting variable expression: %s", expr->value.c_str());
        // --------------------------------------------------
        case ExprType::FUNCTION: {
            if (expr->processed) return;
            if (expr->isConstant) expr->processed = true;
            // We have something like sqrt(...) / sin(...) etc. Possibly multiple args
            //    if (jitLogging) std::cerr << "FUNCTION operator: " << expr->value << std::endl;
            std::string funcName = expr->value;

            // For demonstration, handle single-arg functions sqrt, sin, cos as built-ins
            // For multi-arg (like 'f(a,b)'), you'd handle them differently (a call to a C++ function).
            if (expr->children.empty()) {
                std::cerr << ("Function node with no children? " + funcName) << std::endl;
                SignalHandler::instance().raise(22);
            }

            // Evaluate each argument. Typically the first argument for unary built-ins
            for (auto &child: expr->children) {
                emitExpression(child.get());
            }

            // For the result
            std::string exprTmpName = getUniqueTempName(expr);

            asmjit::x86::Xmm exprReg = getRegister(exprTmpName, expr);

            if (funcName == "sqrt") {
                // Expect 1 child
                Expression *arg = expr->children[0].get();
                std::string argTmpName = getUniqueTempName(arg);
                asmjit::x86::Xmm argReg = getRegister(argTmpName, expr);

                assembler->comment("; Function: sqrt");

                if (exprReg.id() != argReg.id()) {
                    assembler->movaps(exprReg, argReg);
                }

                assembler->sqrtsd(exprReg, exprReg); // in-place sqrt

                incrementUsage(exprTmpName);
                decrementUsage(argTmpName);
            } else {
                // Ensure the function has at least one argument
                if (expr->children.empty()) {
                    std::cerr << "Function node with no children? " << funcName << std::endl;
                    SignalHandler::instance().raise(22);
                }

                // Emit expression for each argument
                for (auto &child: expr->children) {
                    emitExpression(child.get());
                }

                // If we're dealing with unary functions like sqrt, sin, etc.
                if (expr->children.size() == 1) {
                    Expression *arg = expr->children[0].get();
                    std::string argTmpName = getUniqueTempName(arg);
                    asmjit::x86::Xmm argReg = getRegister(argTmpName, arg);

                    // Prepare the result register
                    std::string exprTmpName = getUniqueTempName(expr);
                    //    if (jitLogging) std::cerr << "Generating unique temp name for  expression: " << exprTmpName << std::endl;
                    //    if (jitLogging) std::cerr << "FUNCTION: Reading register map: " << exprTmpName << std::endl;
                    //asmjit::x86::Xmm exprReg = registerMap.at(exprTmpName);
                    asmjit::x86::Xmm exprReg = getRegister(exprTmpName, expr);

                    // Special-case "sqrt" since it has direct assembly support
                    if (funcName == "sqrt") {
                        assembler->comment("; Function: sqrt");
                        if (exprReg.id() != argReg.id()) {
                            assembler->movaps(exprReg, argReg);
                        }

                        assembler->sqrtsd(exprReg, exprReg); // Perform in-place sqrt
                        decrementUsage(argTmpName);
                        incrementUsage(exprTmpName);

                        if (arg->isConstant) {
                            expr->processed = true;
                            assembler->commentf("; That arg was constant");
                        }
                    } else {
                        // Call general math functions via callMathFunction

                        assembler->commentf("; Function: '%s'", funcName.c_str());
                        saveCallerSavedRegisters();
                        callMathFunction(funcName, argReg); // Invoke function
                        assembler->comment("; capture result");


                        if (exprReg.id() != 0) {
                            assembler->movaps(exprReg, asmjit::x86::xmm0); // Capture
                        }

                        incrementUsage(exprTmpName);
                        decrementUsage(argTmpName);
                        restoreCallerSavedRegisters();
                        if (arg->isConstant) {
                            expr->processed = true;
                            assembler->commentf("; That arg was constant");
                        }
                    }
                }
                // If the function takes two arguments (e.g., pow, atan2)
                else if (expr->children.size() == 2) {
                    Expression *arg1 = expr->children[0].get();
                    Expression *arg2 = expr->children[1].get();

                    // Prepare the argument registers
                    std::string arg1TmpName = getUniqueTempName(arg1);
                    //    if (jitLogging) std::cerr << "Generating unique temp name for arg1: " << arg1TmpName << std::endl;
                    std::string arg2TmpName = getUniqueTempName(arg2);
                    //    if (jitLogging) std::cerr << "Generating unique temp name for arg2: " << arg2TmpName << std::endl;

                    //    if (jitLogging) std::cerr << "FUNCTION: Reading register map: " << arg1TmpName << std::endl;
                    // asmjit::x86::Xmm arg1Reg = registerMap.at(arg1TmpName);
                    asmjit::x86::Xmm arg1Reg = getRegister(arg1TmpName, arg1);

                    //    if (jitLogging) std::cerr << "FUNCTION: Reading register map: " << arg2TmpName << std::endl;
                    //  asmjit::x86::Xmm arg2Reg = registerMap.at(arg2TmpName);
                    asmjit::x86::Xmm arg2Reg = getRegister(arg2TmpName, arg2);

                    // Prepare the result register
                    std::string exprTmpName = getUniqueTempName(expr);
                    //    if (jitLogging) std::cerr << "Generating unique temp name for expression: " << exprTmpName << std::endl;
                    //    if (jitLogging) std::cerr << "FUNCTION: Reading register map: " << exprTmpName << std::endl;
                    // asmjit::x86::Xmm exprReg = registerMap.at(exprTmpName);
                    asmjit::x86::Xmm exprReg = getRegister(exprTmpName, expr);

                    // Call general math functions with two arguments via callMathFunction
                    assembler->commentf("; Function: '%s'", funcName.c_str());
                    saveCallerSavedRegisters();
                    callMathFunction(funcName, arg1Reg, arg2Reg);
                    assembler->comment("; capture result");

                    if (exprReg.id() != 0) {
                        assembler->movaps(exprReg, asmjit::x86::xmm0); // Capture
                    }

                    restoreCallerSavedRegisters();

                    decrementUsage(arg1TmpName);
                    decrementUsage(arg2TmpName);
                    if (arg1->isConstant && arg2->isConstant) {
                        expr->processed = true;
                        assembler->commentf("; Those args were constant");
                    }
                } else {
                    // Unsupported number of arguments
                    std::cerr << "Unsupported number of arguments for function: " << funcName
                            << " (expected 1 or 2, got " << expr->children.size() << ")" << std::endl;
                    SignalHandler::instance().raise(22);
                }
                // addCompactRegisterComments
                break;
            }
        }
    }
    for (auto &kids: expr->children) {
        eraseVariablesFromExpression(kids.get());
    }

    assembler->commentf("; Done with expression: %s", expressionToString(expr).c_str());
}


// -----------------------------------------------------------------------------
// Emit a "WHERE" clause: varName = expr
// -----------------------------------------------------------------------------

bool LetCodeGenerator::isConstantExpression(Expression *expr) {
    //    if (jitLogging) std::cerr << "Function isConstantExpression: " << expr->value << std::endl;
    // Base case: Literal values are always constant
    if (expr->type == ExprType::LITERAL) {
        return true;
    }

    // Base case: Variables are NOT constant
    if (expr->type == ExprType::VARIABLE) {
        return false;
    }

    // Functions: Analyze all arguments recursively
    if (expr->type == ExprType::FUNCTION) {
        for (const auto &child: expr->children) {
            if (!isConstantExpression(child.get())) {
                return false; // If any argument is non-constant, the function is not cacheable
            }
        }
        return true; // All arguments are constant
    }

    // Binary operations: Analyze both sides
    if (expr->type == ExprType::BINARY_OP) {
        for (const auto &child: expr->children) {
            if (!isConstantExpression(child.get())) {
                return false; // If any operand is non-constant, the operation is not cacheable
            }
        }
        return true; // Both operands are constant
    }

    // Unary operations: Analyze the single operand
    if (expr->type == ExprType::UNARY_OP) {
        if (!isConstantExpression(expr->children[0].get())) {
            return false; // Operand is non-constant, so the operation is not cacheable
        }
        return true; // Operand is constant
    }

    // By default, assume non-constant (catch-all case)
    return false;
}


void LetCodeGenerator::emitWhereClause(WhereClause *whereClause) {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);


    assembler->commentf("; WHERE: %s = (...)", whereClause->varName.c_str());

    // Check if the RHS expression is constant
    //if ( isConstantExpression(whereClause->expr.get())) {
    if constexpr (true) {
        // where statements are constant.
        // If constant, cache the result as a literal
        assembler->comment("; RHS is constant, caching result");
        emitExpression(whereClause->expr.get());

        std::string rhsTmpName = getUniqueTempName(whereClause->expr.get());
        asmjit::x86::Xmm rhsReg = registerMap.at(rhsTmpName);

        // Store the constant in literalCache
        literalCache[whereClause->varName] = rhsReg;

        // Move it to the LHS register
        asmjit::x86::Xmm lhsReg = registerMap.at(whereClause->varName);

        if (lhsReg.id() != rhsReg.id()) {
            assembler->movaps(lhsReg, rhsReg);
        }

        for (auto &expr: whereClause->expr->children) {
            eraseVariablesFromExpression(expr.get());
        }
        // addCompactRegisterComments
    }
    // Otherwise, emit the expression
    else {
        emitExpression(whereClause->expr.get());
    }
}


// -----------------------------------------------------------------------------
// Load a double literal into an XMM register
// -----------------------------------------------------------------------------
void LetCodeGenerator::emitLoadDoubleLiteral(const std::string &literalString, asmjit::x86::Xmm destReg) {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    // Parse the literal value as a double
    double val = std::stod(literalString);
    assembler->commentf("; using literal %s (value = %f)", literalString.c_str(), val);
    assembler->commentf("; Loading immediate literal %s (value = %f)", literalString.c_str(), val);

    assembler->mov(asmjit::x86::rax, asmjit::Imm(val));
    assembler->movq(destReg, asmjit::x86::rax);
}


void LetCodeGenerator::zeroUsage(const std::string &var) {
    usageTracker[var] = 0;
}

void LetCodeGenerator::incrementUsage(const std::string &var) {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    //assembler->commentf("; Increment usage for variable: %s", var.c_str());
    usageTracker[var]++;
}

void LetCodeGenerator::decrementUsage(const std::string &var) {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    if (usageTracker.count(var) > 0) {
        // Decrement the usage count
        usageTracker[var]--;
     if (usageTracker[var] == 0) {
         freeRegister(var); // Free the associated register/resource
         usageTracker.erase(var); // Remove the variable from `usageTracker`
     }
    } else {
        // Optionally, log a warning if trying to decrement a non-tracked variable
        std::cerr << "Warning: Trying to decrement usage for non-tracked variable: " << var << std::endl;
    }
}


// -----------------------------------------------------------------------------
// allocateRegister
// -----------------------------------------------------------------------------

asmjit::x86::Xmm LetCodeGenerator::allocateRegister(const std::string &var) {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    incrementUsage(var);

    // Check if variable is already in a register
    auto regIt = registerMap.find(var);
    if (regIt != registerMap.end()) {
        return regIt->second;
    }

    // Check if the variable is in spill slots
    if (auto spillIt = spillSlots.find(var); spillIt != spillSlots.end()) {
        return reloadRegister(var, spillIt->second);
    }

    // Allocate a free register if possible
    asmjit::x86::Xmm reg = findFreeRegister();

    if (reg.id() != static_cast<u_int32_t>(-1)) {
        registerMap[var] = reg;
        reverseRegisterMap[reg.id()] = var;
        return reg;
    }

    assembler->commentf("; registers used: %d", registerMap.size());
    // Spill another register if there's no free one
    if (registerMap.size() >= 14) {
        return spillRegister(var);
    }


    return reg;
}

asmjit::x86::Xmm LetCodeGenerator::findFreeRegister() {
    // dont allocate 0,1
    for (int i = 15; i >= 2; --i) {
        asmjit::x86::Xmm reg = asmjit::x86::xmm(i);
        //    if (jitLogging) std::cerr << "Reading reverseRegisterMap map: " << reg.id() << std::endl;
        if (reverseRegisterMap.find(reg.id()) == reverseRegisterMap.end()) {
            return reg; // Found a free register
        }
    }

    return asmjit::x86::xmm(-1); // No free register
}

bool LetCodeGenerator::isConstantValue(const std::string &varName) const {
    return varName.find("_CST_") == 0 || varName.find("_LIT_") == 0;
}


asmjit::x86::Xmm LetCodeGenerator::spillRegister(const std::string &var) {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    std::string candidateVar;
    asmjit::x86::Xmm candidateReg;
    size_t minUsage = SIZE_MAX;

    // Step 1: Look for constants (_CST_ or _LIT_) as first spill candidates
    for (const auto &kv: registerMap) {
        if (isConstantValue(kv.first)) {
            // Prioritize constants
            candidateVar = kv.first;
            candidateReg = kv.second;
            break; // Spill the first constant we find
        }
    }

    // Step 2: If no constants are available, fall back to normal LRU spilling
    if (candidateVar.empty()) {
        for (const auto &kv: registerMap) {
            size_t usageCount = usageTracker[kv.first];

            // Defer variable spilling until all constants are used
            if (!isConstantValue(kv.first) && usageCount < minUsage) {
                minUsage = usageCount;
                candidateVar = kv.first;
                candidateReg = kv.second;
            }
        }
    }

    if (candidateVar.empty()) {
        throw std::runtime_error("No register available to spill!");
    }

    // Step 3: If the chosen candidate is already spilled, skip spilling

    if (spillSlots.find(candidateVar) != spillSlots.end()) {
        assembler->commentf("; '%s' is spilled already.", candidateVar.c_str());

        // Reassign the freed register to the new variable (even though we skip spilling)
        registerMap.erase(candidateVar);
        reverseRegisterMap.erase(candidateReg.id());
        usageTracker[candidateVar]--;

        registerMap[var] = candidateReg;
        reverseRegisterMap[candidateReg.id()] = var;
        usageTracker[var]++;

        return candidateReg;
    }

    // Step 4: Spill the selected candidate (constant or variable)
    size_t spillOffset = spillOffsetCounter;
    spillSlots[candidateVar] = spillOffset;
    spillOffsetCounter += 16;

    assembler->commentf("; Registers used %d - spill '%s' at offset %zu", registerMap.size(), candidateVar.c_str(),
                        spillOffset);

    if (false && contains(var, "_BOP_")) {
        assembler->commentf("; set empty slot for '%s' at offset %zu", var.c_str(), spillOffset);
        // no data to save
    } else {
        assembler->commentf("; Saving '%s' at offset %zu", candidateVar.c_str(), spillOffset);
        assembler->movq(asmjit::x86::rax, candidateReg);
        assembler->mov(asmjit::x86::ptr(asmjit::x86::rdi, spillOffset), asmjit::x86::rax);
    }

    // Remove candidate from register mappings
    registerMap.erase(candidateVar);
    reverseRegisterMap.erase(candidateReg.id());
    usageTracker[candidateVar]--;

    // Assign the freed register to the new variable
    registerMap[var] = candidateReg;
    reverseRegisterMap[candidateReg.id()] = var;
    usageTracker[var]++;

    return candidateReg;
}


void LetCodeGenerator::freeRegister(const std::string &varName) {
    if (usageTracker[varName] > 1) {
        usageTracker[varName]--;
        return;
    }

    auto it = registerMap.find(varName);
    if (it != registerMap.end()) {
        asmjit::x86::Xmm reg = it->second;

        // Remove the variable from register mappings
        registerMap.erase(it);
        reverseRegisterMap.erase(reg.id());

        // Clear cache entry for the freed register
        for (auto cacheIt = literalCache.begin(); cacheIt != literalCache.end();) {
            if (cacheIt->second.id() == reg.id()) {
                cacheIt = literalCache.erase(cacheIt); // Remove the cached literal entry
            } else {
                ++cacheIt;
            }
        }

        if (jitLogging) {
            std::cerr << "; Free value: " << varName
                    << " (Register: xmm" << reg.id() << ")" << std::endl;
        }
    }

    // Clean up any spill slots associated with this variable
    // auto spillIt = spillSlots.find(varName);
    // if (spillIt != spillSlots.end()) {
    //     spillSlots.erase(spillIt);
    //     if (jitLogging) {
    //         std::cerr << "Cleared spill slot associated with variable: " << varName << std::endl;
    //     }
    // }
}

void LetCodeGenerator::allocateGlobalMemory(const std::vector<std::string> &variables) {
    //    if (jitLogging) std::cerr << "Function name allocateGlobalMemory ---------------------" << std::endl;
    // Use the static memory pool
    globalMemory = gGlobalStaticMemory;
    size_t offset = 0;

    // Reserve storage for variables
    for (const auto &var: variables) {
        varOffsets[var] = offset;
        offset += 16; // Allocate 16 bytes per variable
        if (offset > GLOBAL_MEMORY_SIZE) {
            throw std::runtime_error("Global memory pool overflow");
        }
    }

    // Reserve space for spill slots
    spillOffsetCounter = offset;
    if (spillOffsetCounter + (8 * 64) > GLOBAL_MEMORY_SIZE) {
        throw std::runtime_error("Global memory pool overflow during spill slot allocation");
    }
}

void LetCodeGenerator::freeGlobalMemory() {
    //    if (jitLogging) std::cerr << "Function name freeGlobalMemory ---------------------" << std::endl;
    // No need to free static memory; just reset state
    globalMemory = nullptr;
    varOffsets.clear();
}

void LetCodeGenerator::printRegisterMaps() {
    //    if (jitLogging) std::cerr << "Function name printRegisterMaps ---------------------" << std::endl;
    if (!jitLogging) return;
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    assembler->comment("; == Register State ==");
    assembler->comment("; Register Map (Variable -> Register):");
    for (const auto &pair: registerMap) {
        assembler->commentf("; Val: %s = xmm%d [%d]", pair.first.c_str(), pair.second.id(), usageTracker[pair.first]);
    }

    assembler->comment("; Reverse Register Map (Register ID -> Variable):");
    for (const auto &pair: reverseRegisterMap) {
        assembler->commentf("; Val: %s = xmm%d [%d]", pair.first, pair.second.c_str(), usageTracker[pair.second]);
    }

    assembler->comment("; Spill Slots:");
    for (const auto &pair: spillSlots) {
        assembler->commentf("; Variable: %s, Spill Offset: %zu", pair.first.c_str(), pair.second);
    }
}


asmjit::x86::Xmm LetCodeGenerator::getRegister(const std::string &varName, [[maybe_unused]] Expression *expr) {
    if (auto regIt = registerMap.find(varName); regIt != registerMap.end()) {
        return regIt->second;
    }

    if (auto spillIt = spillSlots.find(varName); spillIt != spillSlots.end()) {
        return reloadRegister(varName, spillIt->second); // Reload spilled variable
    }

    // Otherwise, allocate a new register
    return allocateRegister(varName);
}

void LetCodeGenerator::addCompactRegisterComments() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    // Create a single-line comment summarizing all variable-to-register mappings
    std::ostringstream commentStream;
    commentStream << "; Registers: ";

    for (const auto &entry: registerMap) {
        const std::string &varName = entry.first; // Variable name
        const asmjit::x86::Xmm &reg = entry.second; // Corresponding register
        commentStream << varName << "=xmm" << reg.id() << ", ";
    }

    // Convert stream to string and remove trailing ", "
    std::string comment = commentStream.str();
    if (comment.size() > 2) {
        comment = comment.substr(0, comment.size() - 2); // Remove trailing ", "
    }

    // Emit the single-line comment
    assembler->comment(comment.c_str());
}


void LetCodeGenerator::addRegisterComments() {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    // Comment to separate the section for variable/register mapping
    assembler->comment("; === Register Usage Summary ===");

    // Current mapping of variables to XMM registers
    assembler->comment("; Variables and their corresponding XMM registers:");
    for (const auto &entry: registerMap) {
        const std::string &varName = entry.first; // Variable name
        const asmjit::x86::Xmm &reg = entry.second; // Corresponding register
        assembler->commentf("; V: %s -> R: xmm%d U: %d", varName.c_str(), reg.id(), usageTracker[varName]);
    }

    // Current mapping of XMM registers to variables (reverse lookup)
    assembler->comment("; XMM registers and their mapped variables:");
    for (const auto &entry: reverseRegisterMap) {
        int regId = entry.first; // Register ID
        const std::string &varName = entry.second; // Corresponding variable
        assembler->commentf("; R: xmm%d -> V: %s U: %d", regId, varName.c_str(), usageTracker[varName]);
    }

    // Spill slots
    assembler->comment("; Spill slots and their corresponding variables:");
    for (const auto &entry: spillSlots) {
        const std::string &varName = entry.first;
        size_t spillOffset = entry.second;
        assembler->commentf("; Variable: %s -> Spill Offset: %zu", varName.c_str(), spillOffset);
    }

    assembler->comment("; ===============================");
}


asmjit::x86::Xmm LetCodeGenerator::reloadSpilledVariable(const std::string &varName) {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return asmjit::x86::Xmm(0);

    auto it = registerMap.find(varName);
    if (it != registerMap.end()) {
        assembler->commentf("; '%s' is already in register. Skipping reload", varName.c_str());
        return it->second; // Return the existing register
    }

    auto spillIt = spillSlots.find(varName);
    if (spillIt == spillSlots.end()) {
        std::cerr << "Error: Attempting to reload a variable that is not spilled: " << varName << std::endl;
        SignalHandler::instance().raise(22); // Signal a critical error
    }
    return allocateRegister(varName); // Assume this handles reloading
}

void LetCodeGenerator::spillVariable(const std::string &var, asmjit::x86::Xmm reg) {
    asmjit::x86::Assembler *assembler;
    if (initialize_assembler(assembler)) return;

    // dont spill values that dont really exist yet
    if (preloading) return;

    // Check if the variable is already spilled
    if (spillSlots.find(var) != spillSlots.end()) {
        assembler->commentf("; '%s' is already spilled. Skipping redundant spill", var.c_str());
        return;
    }

    // Check if the variable is a constant
    if (isConstantValue(var)) {
        // If the constant is already spilled, skip the spill
        if (spillSlots.find(var) != spillSlots.end()) {
            assembler->commentf("; Constant '%s' is already spilled. Skipping redundant spill", var.c_str());
            return;
        }
    }


    size_t spillOffset = spillOffsetCounter;
    spillSlots[var] = spillOffset;
    spillOffsetCounter += 16; // Spill 16 byte aligned


    assembler->commentf("; Spill: '%s' to slot: %zu", var.c_str(), spillOffset);
    assembler->movq(asmjit::x86::rax, reg);
    assembler->mov(asmjit::x86::ptr(asmjit::x86::rdi, spillOffset), asmjit::x86::rax);

    // Cleanup register mappings
    registerMap.erase(var);
    reverseRegisterMap.erase(reg.id());
}

asmjit::x86::Xmm LetCodeGenerator::reloadRegister(const std::string &var, size_t spillOffset) {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    asmjit::x86::Xmm reg = findFreeRegister();

    // Spill another variable if no free register exists
    if (reg.id() == static_cast<u_int32_t>(-1)) {
        reg = spillRegister(var); // Will handle variable spilling
    }

    assembler->commentf("; slurp '%s' from slot %zu", var.c_str(), spillOffset);
    assembler->movsd(reg, asmjit::x86::ptr(asmjit::x86::rdi, spillOffset));

    registerMap[var] = reg;
    reverseRegisterMap[reg.id()] = var;
    spillSlots.erase(var); // Clear spill info

    return reg;
}

void LetCodeGenerator::clearMappings(const std::string &var, asmjit::x86::Xmm reg) {
    registerMap.erase(var);
    reverseRegisterMap.erase(reg.id());
    spillSlots.erase(var);
    usageTracker.erase(var); // Clear usage tracker for the variable
}

void LetCodeGenerator::emitRegisterSave() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    int offset = 0;
    for (int regId = 0; regId < 8; ++regId) {
        if (reverseRegisterMap.find(regId) == reverseRegisterMap.end()) continue;

        assembler->commentf("; Save xmm%d to spill slot %zu", regId, offset);
        assembler->movaps(asmjit::x86::ptr(asmjit::x86::rdi, offset), asmjit::x86::xmm(regId));
        offset += 16;
    }
}

void LetCodeGenerator::emitRegisterRestore() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    int offset = 0;
    for (int regId = 0; regId < 8; ++regId) {
        if (reverseRegisterMap.find(regId) == reverseRegisterMap.end()) continue;

        assembler->commentf("; Restore xmm%d from spill slot %zu", regId, offset);
        assembler->movaps(asmjit::x86::xmm(regId), asmjit::x86::ptr(asmjit::x86::rdi, offset));
        offset += 16;
    }
}

void LetCodeGenerator::emitSpillAndReloadForArguments(const std::vector<Expression *> &args) {
    for (auto &arg: args) {
        emitExpression(arg); // Make sure the argument emits its code
        std::string argTmpName = getUniqueTempName(arg);
        getRegister(argTmpName, arg); // Ensure the temp has a register
    }
}

bool LetCodeGenerator::isVariableSpilled(const std::string &varName) {
    return spillSlots.find(varName) != spillSlots.end();
}

asmjit::x86::Xmm LetCodeGenerator::findRegisterForVariable(const std::string &varName) {
    auto regIt = registerMap.find(varName);
    if (regIt == registerMap.end()) {
        std::cerr << "Error: No register assigned for variable: " << varName << std::endl;
        //SignalHandler::instance().raise(22); // Signal a critical error
        return asmjit::x86::Xmm(0);
    }
    return regIt->second;
}

std::string LetCodeGenerator::findVariableForRegister(const asmjit::x86::Xmm &xmmReg) {
    auto varIt = reverseRegisterMap.find(xmmReg.id());
    if (varIt == reverseRegisterMap.end()) {
        std::cerr << "Error: No variable associated with register: " << xmmReg.id() << std::endl;
        // SignalHandler::instance().raise(22); // Signal a critical error
        return "unknown";
    }
    return varIt->second; // Return the variable name
}


asmjit::x86::Xmm LetCodeGenerator::findOrAllocateTempRegister(const std::string &tmpName, Expression *expr,
                                                              const std::string &varName) {
    auto tmpIt = registerMap.find(tmpName);
    if (tmpIt == registerMap.end()) {
        // No temp register found; log a warning
        std::cerr << "Warning: Temp register for variable '" << varName
                << "' expression was not found: " << tmpName << std::endl;
        std::cerr << "Expression value: " << expr->value << std::endl;
    }
    return getRegister(tmpName, expr);
}

void LetCodeGenerator::copyVariableToTempRegister(const std::string &varName, const std::string &tmpName,
                                                  asmjit::x86::Xmm varReg, asmjit::x86::Xmm tmpReg) {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    if (tmpReg.id() != varReg.id()) {
        assembler->commentf("; Copy variable '%s' -> temporary register '%s'", varName.c_str(), tmpName.c_str());

        if (tmpReg.id() != varReg.id()) {
            assembler->movaps(tmpReg, varReg); // Actual copy operation
        }
    }
}

// BINARY OP

asmjit::x86::Xmm LetCodeGenerator::emitChildExpression(Expression *childExpr) {
    emitExpression(childExpr); // Emit code for the child
    std::string tmpName = getUniqueTempName(childExpr); // Get unique temp name
    incrementUsage(tmpName);
    return getRegister(tmpName, childExpr); // Get or allocate register for child
}

void LetCodeGenerator::emitBinaryOperation(const std::string &op,
                                           const asmjit::x86::Xmm &exprReg,
                                           const asmjit::x86::Xmm &lhsReg,
                                           const asmjit::x86::Xmm &rhsReg,
                                           [[maybe_unused]] Expression *lhsExpr,
                                           [[maybe_unused]] Expression *rhsExpr) {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    if (op == "+") {
        assembler->addsd(exprReg, rhsReg);
    } else if (op == "-") {
        assembler->subsd(exprReg, rhsReg);
    } else if (op == "*") {
        assembler->mulsd(exprReg, rhsReg);
    } else if (op == "/") {
        assembler->divsd(exprReg, rhsReg);
    } else if (op == "^") {
        emitExponentiation(exprReg, lhsReg, rhsReg); // Encapsulate "^" logic
    } else {
        std::cerr << "Unsupported binary operator: " << op << std::endl;
        SignalHandler::instance().raise(22); // Handle invalid operators
    }
    decrementUsageByRegister(rhsReg);
}

void LetCodeGenerator::emitExponentiation(asmjit::x86::Xmm exprReg,
                                          asmjit::x86::Xmm lhsReg,
                                          asmjit::x86::Xmm rhsReg) {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    assembler->comment("; Exponentiation");

    // Load constant 2.0 into a register
    asmjit::x86::Xmm twoReg = allocateRegister("_const_2");
    emitLoadDoubleLiteral("2.0", twoReg);

    // Compare rhsReg (exponent) with 2.0
    assembler->ucomisd(rhsReg, twoReg);
    decrementUsage("_const_2");

    // Fast path for x^2
    asmjit::Label usePow = assembler->newLabel();
    assembler->jne(usePow); // Jump to "slow path" if y != 2

    if (lhsReg.id() == rhsReg.id()) {
        assembler->movaps(exprReg, lhsReg);
    }

    assembler->mulsd(exprReg, lhsReg);

    // Skip pow(x, y) call
    asmjit::Label done = assembler->newLabel();
    assembler->jmp(done);

    // Slow path: Use pow(x, y)
    assembler->bind(usePow);
    saveCallerSavedRegisters();
    callMathFunction("pow", lhsReg, rhsReg); // Call pow() with lhs and rhs

    if (exprReg.id() != 0) {
        assembler->movaps(exprReg, asmjit::x86::xmm0); // Save result
    }

    decrementUsageByRegister(lhsReg);
    decrementUsageByRegister(rhsReg);
    restoreCallerSavedRegisters();

    // Mark operation as complete
    assembler->bind(done);
}

void LetCodeGenerator::logJitOperation(const std::string &message) {
    if (jitLogging) {
        std::cerr << message << std::endl;
    }
}

void LetCodeGenerator::decrementUsageByRegister(asmjit::x86::Xmm reg) {
    // Find the temporary name associated with the register
    auto it = reverseRegisterMap.find(reg.id());
    if (it == reverseRegisterMap.end()) {
        // std::cerr << ("Register not found in reverseRegisterMap. Cannot decrement usage.");
        return;
    }

    const std::string &tempName = it->second;

    // Check if the tempName exists in usageTracker
    auto usageIt = usageTracker.find(tempName);
    if (usageIt != usageTracker.end()) {
        // Decrement usage count
        usageIt->second--;

        // If usage reaches zero, free the register
        if (usageIt->second == 0) {
            // Remove from registerMap and reverseRegisterMap
            registerMap.erase(tempName);
            reverseRegisterMap.erase(it);
            // Remove from usageTracker entirely if not in use anymore
            usageTracker.erase(usageIt);
        }
    } else {
        // Usage tracking data mismatch; handle this gracefully
        // throwing exceptions for this is not graceful!!!
        std::cerr << "Usage tracking data mismatch for variable: " << tempName << std::endl;
    }
}


std::string LetCodeGenerator::expressionToString(Expression *expr) {
    if (!expr) return "<null>";

    switch (expr->type) {
        case ExprType::LITERAL:
            return expr->value; // A literal, e.g., "42.0"
        case ExprType::VARIABLE:
            return expr->value; // A variable name, e.g., "x"
        case ExprType::CONSTANT:
            return expr->value; // A variable name, e.g., "x"
        case ExprType::UNARY_OP:
            // Unary operation, like negation
            return "(" + expr->value + " " + expressionToString(expr->children[0].get()) + ")";
        case ExprType::BINARY_OP:
            // Binary operation, like addition
            return "(" + expressionToString(expr->children[0].get()) + " " + expr->value + " " +
                   expressionToString(expr->children[1].get()) + ")";
        case ExprType::FUNCTION:
            // Function call, like sin(x)
        {
            std::string funcExpr = expr->value + "(";
            for (size_t i = 0; i < expr->children.size(); ++i) {
                funcExpr += expressionToString(expr->children[i].get());
                if (i < expr->children.size() - 1) funcExpr += ", ";
            }
            funcExpr += ")";
            return funcExpr;
        }
        default:
            return "<unknown expression>";
    }
}
