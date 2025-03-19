#include "LetCodeGenerator.h"
#include <asmjit/asmjit.h> // Include assembler support
#include <immintrin.h>
#include <cmath>


// Initialize static methods and private constructors

LetCodeGenerator::LetCodeGenerator() {
}


void LetCodeGenerator::initialize() {
    RegisterTracker::instance().initialize();
    variables.clear();
}

void LetCodeGenerator::generateCode(const ASTNode *node) {
    jitLogging = true;
    emitFunctionPrologue(); // Prologue example
    generateLetStatement(dynamic_cast<const LetStatement *>(node));
    printRegisterUsage();
    jitLogging = false;
}

void LetCodeGenerator::saveGPcache(asmjit::x86::Assembler *assembler) {
    assembler->comment("; -- save R12-R15 for GP cache");
    assembler->push(asmjit::x86::r12);
    assembler->push(asmjit::x86::r13);
    assembler->push(asmjit::x86::r14);
    assembler->push(asmjit::x86::r15);
}

void LetCodeGenerator::restoreGPCache(asmjit::x86::Assembler *assembler) {
    // restore GP cache
    assembler->comment("; -- restore R12-R15 for GP cache");
    assembler->pop(asmjit::x86::r15);
    assembler->pop(asmjit::x86::r14);
    assembler->pop(asmjit::x86::r13);
    assembler->pop(asmjit::x86::r12);
}


void LetCodeGenerator::generateLetStatement(const LetStatement *letStmt) {
    if (!letStmt) return;
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    // Code generation logic for LET statement
    if (debug) std::cerr << "// Input Parameters\n";

    if (GPCACHE) {
        tracker.enableGpCache();
    }

    if (TrackLRU) {
        tracker.enableLRU();
    }

    loadArguments(letStmt->inputParams);

    // after loading we save
    if (GPCACHE) {
        saveGPcache(assembler);
    }

    if (debug) std::cerr << "// WHERE Clauses\n";
    for (const auto &wc: letStmt->whereClauses) {
        generateWhereClause(wc.get());
    }

    if (debug) std::cerr << "// Expressions\n";
    size_t numExprs = letStmt->expressions.size();
    for (size_t i = numExprs; i-- > 0;) {
        Expression *expr = letStmt->expressions[i].get();
        generateExpression(expr);
        if (debug) std::cerr << "STORE_VAR " << letStmt->outputVars[i] << "\n";
        auto name = getUniqueTempName(expr);
        if (debug) std::cerr << "EXPR NAME: " << name << "\n";
        asmjit::x86::Xmm exprReg = tracker.allocateRegister(name);
        assembler->commentf("; constant result in: %d", exprReg.id());
        tracker.setConstant(name);
    }

    if (GPCACHE) {
        restoreGPCache(assembler);
    }

    // save to forth stack at the end
    for (size_t i = numExprs; i-- > 0;) {
        const Expression *expr = letStmt->expressions[i].get();
        auto name = getUniqueTempName(expr);
        if (debug) std::cerr << "Save to stack: " << name << "\n";
        asmjit::x86::Xmm exprReg = tracker.allocateRegister(name); // will reload.
        assembler->commentf("; Pushing result of '%s' onto stack", letStmt->outputVars[i].c_str());
        assembler->sub(asmjit::x86::r15, 8); // Allocate space on the stack
        assembler->mov(asmjit::x86::ptr(asmjit::x86::r15), asmjit::x86::r12);
        assembler->mov(asmjit::x86::r12, asmjit::x86::r13);
        assembler->movq(asmjit::x86::r13, exprReg);
    }

    assembler->pop(asmjit::x86::rdi);
}

void LetCodeGenerator::generateWhereClause(const WhereClause *wc) {
    if (!wc) return;

    if (debug) std::cerr << wc->varName << " = ";
    generateExpression(wc->expr.get());

    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    asmjit::x86::Xmm varReg = tracker.allocateRegister(wc->varName);

    auto exprName = getUniqueTempName(wc->expr.get());
    auto exprReg = tracker.allocateRegister(exprName);
    if (exprReg.id() != varReg.id()) {
        assembler->movaps(varReg, exprReg);
    }
    // frees the exprReg after moving to varReg
    tracker.freeRegister(exprName);
    if (debug)
        std::cerr << "[DEBUG] WHERE variable '" << wc->varName
                << "' stored in register xmm" << varReg.id() << "\n";


    if (debug) std::cerr << "// End WHERE Clause\n";
}

void LetCodeGenerator::generateExpression(Expression *expr) {
    if (!expr) return;

    switch (expr->type) {
        case ExprType::LITERAL:
            generateLiteralExpr(expr);
            break;
        case ExprType::CONSTANT:
        case ExprType::VARIABLE:
            generateVariableExpr(expr);
            break;
        case ExprType::FUNCTION:
            generateFunctionExpr(expr);
            break;
        case ExprType::BINARY_OP:
            generateBinaryOpExpr(expr);
            break;
        case ExprType::UNARY_OP:
            generateUnaryOpExpr(expr);
            break;
        default:
            if (debug) std::cerr << "Unknown expression type in generateExpression.\n";
    }
}

void LetCodeGenerator::generateLiteralExpr(const Expression *expr) {
    commentOnExpression(expr);
    // Ensure the expression is valid and of type Literal
    if (!expr || expr->type != ExprType::LITERAL) {
        if (debug) std::cerr << "[ERROR] Attempted to generate code for a non-literal expression.\n";
        return;
    }

    // Extract the literal value from the AST expression
    std::string literalValue = expr->value;


    // Generate a unique name based on the literal value (or reuse if already loaded)
    std::string constName = getUniqueTempName(expr);

    // Insert the constant into constant tracking
    tracker.setConstant(constName);

    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler); // Ensure assembler is initialized.

    // Allocate a register for this constant through the tracker
    asmjit::x86::Xmm xmmReg = tracker.allocateRegister(constName);

    // Convert the literal value (string) into a double (assuming this is a numeric/floating-point constant).
    double value = std::stod(literalValue);

    assembler->mov(asmjit::x86::rax, asmjit::Imm(value));
    assembler->movq(xmmReg, asmjit::x86::rax);

    // Emit debug output for the allocation
    if (debug)
        std::cerr << "[DEBUG] Allocated register " << tracker.xmmRegToStr(xmmReg.id())
                << " for constant value: " << literalValue << "\n";

    // Emit code to load the constant into the allocated register
    if (debug) std::cerr << "LOAD_CONST " << constName << " = " << literalValue << "\n";
}

void LetCodeGenerator::generateVariableExpr(const Expression *expr) {
    // Check if the given expression is a constant
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler); // Ensure assembler is initialized.
    std::string argName = getUniqueTempName(expr);
    asmjit::x86::Xmm xmmReg = tracker.allocateRegister(argName);
    std::string varName = expr->value;
    if (debug) std::cerr << "LOAD_VAR " << varName << "\n";
    asmjit::x86::Xmm xmmRegSrc = tracker.allocateRegister(varName);
    if (xmmReg.id() != xmmRegSrc.id()) {
        assembler->movaps(xmmReg, xmmRegSrc);
    }
    // must not free argName, can free varName, but seems wrong :)
    // tracker.freeRegister(varName);
}


void LetCodeGenerator::commentOnExpression(const Expression *expr) {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    std::string sourceText = "; evaluating: " + expressionToText(expr);
    assembler->comment(sourceText.c_str());
}

void LetCodeGenerator::generateFunctionExpr(const Expression *expr) {
    if (!expr) {
        if (debug) std::cerr << "Error: Null expression passed to generateFunctionExpr.\n";
        return;
    }
    commentOnExpression(expr);
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    // Extract the function name from the expression
    const std::string &funcName = expr->value;

    // Ensure the expression has children (arguments to the function)
    if (expr->children.empty()) {
        if (debug) std::cerr << "Error: Function " << funcName << " called with no arguments.\n";
        SignalHandler::instance().raise(22); // Error signal
        return;
    }

    // A mapping from variable names to allocated registers for freeing later
    std::vector<std::pair<std::string, asmjit::x86::Xmm> > argNameToReg;

    // Generate code for child expressions (function arguments).
    for (size_t i = 0; i < expr->children.size(); ++i) {
        auto childExpr = expr->children[i].get();

        // Allocate a register for the argument
        std::string argName = getUniqueTempName(childExpr);
        asmjit::x86::Xmm argReg = RegisterTracker::instance().allocateRegister(argName);

        // Generate assembly for the argument and place the result in the allocated register
        generateExpression(childExpr);

        // Store the mapping of argument names to their registers
        argNameToReg.emplace_back(argName, argReg);
    }

    // square root
    if (funcName == "sqrt") {
        assembler->sqrtsd(asmjit::x86::xmm0, argNameToReg[0].second);

    } else if (funcName == "hypot") {
        assembler->comment("; Hypotenuse");
        asmjit::Label hypot_done = assembler->newLabel();
        auto xReg = tracker.allocateRegister("_x");
        auto yReg = tracker.allocateRegister("_y");
        auto tempReg = tracker.allocateRegister("_temp");
        auto ratioReg = tracker.allocateRegister("_ratio");
        auto oneReg = tracker.allocateRegister("_one");
        auto maskReg = tracker.allocateRegister("_mask");

        assembler->comment("; Load mask for fabs (0x7FFFFFFFFFFFFFFF)");
        // Load mask for fabs (0x7FFFFFFFFFFFFFFF)
        asmjit::x86::Gp transfer = asmjit::x86::rax;
        assembler->mov(transfer, asmjit::Imm(0x7FFFFFFFFFFFFFFF));
        assembler->comment("; fabs(x) and fabs(y)");

        // fabs(x) and fabs(y)
        assembler->movq(maskReg, transfer);
        assembler->andpd(xReg, maskReg);
        assembler->andpd(yReg, maskReg);

        // Ensure x >= y (swap if necessary)
        assembler->comment("; Ensure x >= y");
        assembler->movapd(tempReg, xReg);   // temp = x
        assembler->maxsd(xReg, yReg);       // x = max(x, y)
        assembler->minsd(yReg, tempReg);    // y = min(temp, y)


        // Check if y == 0, return x
        assembler->comment("; Check if y == 0, return x");

        assembler->mov(transfer, asmjit::Imm(0)); // 0.0 in double precision
        assembler->movq(maskReg, transfer); // re use maskReg as "zero constant"
        assembler->comisd(yReg, maskReg);
        assembler->je(hypot_done);

        // ratio = y/x
        assembler->comment("; ratio = y/x");
        assembler->divsd(yReg, xReg);
        assembler->movapd(ratioReg, yReg);

        // Compute sqrt(1 + ratio²)
        assembler->comment("Compute sqrt(1 + ratio²)");

        assembler->mov(transfer, asmjit::Imm(4607182418800017408)); // 1.0
        assembler->movq(oneReg, transfer);
        assembler->vfmadd213sd(ratioReg, ratioReg, oneReg);
        assembler->sqrtsd(ratioReg, ratioReg); // sqrt(1 + ratio²)

        // result = x * sqrt(1 + ratio²)
        assembler->comment("; result = x * sqrt(1 + ratio²)");
        assembler->mulsd(ratioReg, xReg);
        assembler->comment("; Label done");
        assembler->bind(hypot_done);
        assembler->movaps(asmjit::x86::xmm0, ratioReg);

        tracker.freeRegister("_x");
        tracker.freeRegister("_y");
        tracker.freeRegister("_temp");
        tracker.freeRegister("_ratio");
        tracker.freeRegister("_one");
        tracker.freeRegister("_mask");
        // Free the registers for the argument names.

    } else

    // Check argument count and call the appropriate math function
        if (argNameToReg.size() == 1) {
            callMathFunction(funcName, argNameToReg[0].second, asmjit::x86::Xmm()); // Single argument
        } else if (argNameToReg.size() == 2) {
            callMathFunction(funcName, argNameToReg[0].second, argNameToReg[1].second); // Two arguments
        } else {
            if (debug)
                std::cerr << "Error: The function " << funcName
                        << " requires one or two arguments, but received " << argNameToReg.size() << ".\n";
            SignalHandler::instance().raise(22); // Error signal
        }
    auto name = getUniqueTempName(expr);
    asmjit::x86::Xmm exprReg = RegisterTracker::instance().allocateRegister(name);
    // capture the result from the function
    if (exprReg.id() != 0) {
        assembler->movaps(exprReg, asmjit::x86::xmm0); // Capture
    }
    // Frees the registers for the argument names.
    for (const auto &pair: argNameToReg) {
        RegisterTracker::instance().freeRegister(pair.first); // Free register by its variable name
    }
}


void LetCodeGenerator::generateUnaryOpExpr(const Expression *expr) {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    commentOnExpression(expr);
    // Validate the input expression
    if (!expr) {
        if (debug) std::cerr << "Error: Null expression passed to generateUnaryOpExpr." << std::endl;
        return;
    }

    // Ensure the expression type is unary
    if (expr->type != ExprType::UNARY_OP) {
        if (debug) std::cerr << "Error: Expression type is not a unary operation." << std::endl;
        return;
    }

    // Ensure there is a child expression
    if (expr->children.empty()) {
        if (debug) std::cerr << "Error: Unary operation requires one operand." << std::endl;
        return;
    }

    // Get the child expression
    Expression *child = expr->children[0].get();

    // Emit the child expression's code
    generateExpression(child);

    // Generate unique names for the temporary registers
    std::string childTmpName = getUniqueTempName(child);
    std::string exprTmpName = getUniqueTempName(expr);

    // Allocate registers for the child and this expression
    asmjit::x86::Xmm childReg = tracker.allocateRegister(childTmpName);
    asmjit::x86::Xmm exprReg = tracker.allocateRegister(exprTmpName);

    // Check the value of the unary operation
    if (expr->value == "neg") {
        // Handle negation
        assembler->comment("; Unary negation");

        // Move the child result into exprReg if needed
        if (exprReg.id() != childReg.id()) {
            assembler->movaps(exprReg, childReg);
        }

        // Load 0.0 into a temporary register
        asmjit::x86::Xmm zeroReg = tracker.allocateRegister("_zero");
        emitLoadDoubleLiteral("0.0", zeroReg);

        // Perform the negation operation (exprReg = 0.0 - child)
        assembler->subsd(exprReg, exprReg); // exprReg = exprReg - exprReg => 0.0
        assembler->subsd(exprReg, childReg); // exprReg = 0.0 - childReg => -childReg
        tracker.freeRegister("_zero");
        tracker.freeRegister(childTmpName); // test
    } else {
        // Unknown unary operator
        if (debug) std::cerr << "Unknown unary operator: " << expr->value << std::endl;
        SignalHandler::instance().raise(22); // Raise signal for unknown operator
    }
}

void LetCodeGenerator::generateBinaryOpExpr(Expression *expr) {
    // Validate input expression
    if (!expr) {
        if (debug) std::cerr << "Error: Null expression passed to generateBinaryOpExpr." << std::endl;
        return;
    }
    commentOnExpression(expr);
    // Ensure the expression type is a binary operation
    if (expr->type != ExprType::BINARY_OP) {
        if (debug) std::cerr << "Error: Expression type is not a binary operation." << std::endl;
        return;
    }

    // Ensure there are at least two children (lhs and rhs)
    if (expr->children.size() < 2) {
        if (debug) std::cerr << "Error: Binary operation requires two operands (lhs, rhs)." << std::endl;
        return;
    }

    // Extract the left-hand side (lhs) and right-hand side (rhs) operands
    Expression *lhsExpr = expr->children[0].get(); // First child is lhs
    Expression *rhsExpr = expr->children[1].get(); // Second child is rhs

    // Extract the operator symbol
    std::string op = expr->value; // Operator symbol is stored in the 'value' field

    // Generate unique names for lhs, rhs, and the result expressions
    std::string lhsVarName = getUniqueTempName(lhsExpr);
    std::string rhsVarName = getUniqueTempName(rhsExpr);
    std::string resultVarName = getUniqueTempName(expr);

    // Allocate registers using unique names
    RegisterTracker &regTracker = RegisterTracker::instance();
    asmjit::x86::Xmm lhsReg = regTracker.allocateRegister(lhsVarName);
    asmjit::x86::Xmm rhsReg = regTracker.allocateRegister(rhsVarName);
    asmjit::x86::Xmm resultReg = regTracker.allocateRegister(resultVarName);

    // Generate assembly to evaluate left-hand side operand
    generateExpression(lhsExpr); // Only emits code for lhsExpr; result is placed in lhsReg

    // Generate assembly to evaluate right-hand side operand
    generateExpression(rhsExpr); // Only emits code for rhsExpr; result is placed in rhsReg

    // Emit the binary operation code
    emitBinaryOperation(op, resultReg, lhsReg, rhsReg, lhsExpr, rhsExpr);

    // Free temporary operand registers
    regTracker.freeRegister(lhsVarName);
    regTracker.freeRegister(rhsVarName);

    // IMPORTANT: Do not free resultVarName here if the result is still needed elsewhere.
}


void LetCodeGenerator::emitFunctionPrologue() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);
    assembler->push(asmjit::x86::rdi); // Save RDI (because it's overwritten)
    setupSpillSlotBase();
}

// Static method for setting up the RDI spill slot base
void LetCodeGenerator::setupSpillSlotBase() {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    // Get the spill base address from RegisterTracker
    void *spillSlotBase = RegisterTracker::instance().getSpillSlotBase();
    assembler->mov(asmjit::x86::rdi, reinterpret_cast<uintptr_t>(spillSlotBase));
}

void LetCodeGenerator::emitLoadDoubleLiteral(const std::string &literalString, asmjit::x86::Xmm destReg) {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    double val = std::stod(literalString);
    assembler->mov(asmjit::x86::rax, asmjit::Imm(val));
    assembler->movq(destReg, asmjit::x86::rax); // Move double into register
}

void LetCodeGenerator::printRegisterUsage() const {
    tracker.printRegisterStatus();
}


void LetCodeGenerator::preserveAndCallFunction(void *func) {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    assembler->push(asmjit::x86::rdi); // Preserve rdi register
    assembler->call(asmjit::imm(func)); // Call the math function
    assembler->pop(asmjit::x86::rdi); // Restore rdi register
}


void LetCodeGenerator::emitExponentiation(asmjit::x86::Xmm exprReg,
                                          asmjit::x86::Xmm lhsReg,
                                          asmjit::x86::Xmm rhsReg) {
    asmjit::x86::Assembler *assembler;
    initialize_assembler(assembler);

    assembler->comment("; Exponentiation");


    // Load constant 2.0 into a register
    asmjit::x86::Xmm twoReg = tracker.allocateRegister("_const_2");
    constexpr const char *CONST_TWO = "2.0"; // Centralized constant
    emitLoadDoubleLiteral(CONST_TWO, twoReg);

    // Compare rhsReg (exponent) with 2.0
    assembler->ucomisd(rhsReg, twoReg);

    // Fast path: Optimize for x^2
    asmjit::Label usePow = assembler->newLabel(); // Label for slow path
    assembler->jne(usePow); // Jump to slow path (general pow(a, b)) if exponent != 2

    // Fast path: Compute x^2
    if (lhsReg.id() == rhsReg.id()) {
        // Ensure x^2 is computed even if lhs == rhs
        assembler->movaps(exprReg, lhsReg);
    }
    assembler->mulsd(exprReg, lhsReg); // expr = lhs * lhs

    // Skip pow(x, y) call
    asmjit::Label done = assembler->newLabel(); // Label for jump after slow path
    assembler->jmp(done);

    // Slow path: Using pow(x, y)
    assembler->bind(usePow);
    callMathFunction("pow", lhsReg, rhsReg); // Call pow(lhs, rhs)

    // Store result into exprReg
    if (exprReg.id() != 0) {
        // check if exprReg is already xmm0
        assembler->movaps(exprReg, asmjit::x86::xmm0); // Save pow result
    }

    // Mark operation as complete
    assembler->bind(done);
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
    {"log10", &log10}
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


void LetCodeGenerator::callMathFunction(const std::string &funcName, const asmjit::x86::Xmm &arg1Reg,
                                        const asmjit::x86::Xmm &arg2Reg) {
    asmjit::x86::Assembler *assembler = nullptr; // Ensure assembler pointer initialized
    if (initialize_assembler(assembler)) {
        if (debug) std::cerr << "Failed to initialize assembler in callMathFunction." << std::endl;
        return;
    }

    assembler->commentf("; ====== call to C math: %s", funcName.c_str());

    // Check single-argument functions
    auto singleIt = SingleFuncMap.find(funcName);
    if (singleIt != SingleFuncMap.end()) {
        if (!arg1Reg.isValid()) {
            if (debug)
                std::cerr << "Invalid register for argument 1 in single-argument function: "
                        << funcName << std::endl;
            SignalHandler::instance().raise(22); // Error signal
            return;
        }
        assembler->commentf("; pre call spill used registers");
        tracker.spillRegisters(); // spill any caller save registers
        assembler->movaps(asmjit::x86::xmm0, arg1Reg); // Move arg1 to xmm0
        preserveAndCallFunction(reinterpret_cast<void *>(singleIt->second));
        assembler->commentf("; post call reload used registers");
        tracker.reloadRegisters(); // reload any caller save registers
        return;
    }

    // Check dual-argument functions
    auto dualIt = DualFuncMap.find(funcName);
    if (dualIt != DualFuncMap.end()) {
        if (!arg1Reg.isValid() || !arg2Reg.isValid()) {
            if (debug)
                std::cerr << "Dual-argument function requires two"
                        " valid arguments: "
                        << funcName << std::endl;
            SignalHandler::instance().raise(22); // Error signal
            return;
        }
        assembler->commentf("; pre call spill used registers");
        assembler->movaps(asmjit::x86::xmm0, arg1Reg); // Move arg1 to xmm0
        assembler->movaps(asmjit::x86::xmm1, arg2Reg); // Move arg2 to xmm1
        preserveAndCallFunction(reinterpret_cast<void *>(dualIt->second));
        assembler->commentf("; post call reload used registers");

        return;
    }

    // Unknown function
    if (debug) std::cerr << "Unknown function: " << funcName << ". Check function maps." << std::endl;
    SignalHandler::instance().raise(22); // Error signal
}


bool LetCodeGenerator::isConstantExpression(Expression *expr) {
    if (!expr) {
        return false; // Null expressions cannot be constant
    }

    switch (expr->type) {
        case ExprType::LITERAL:
            return true; // Literals are inherently constant

        case ExprType::VARIABLE:
            return false; // Variables are not constant

        case ExprType::FUNCTION: {
            // Functions are constant if all their arguments are constant
            for (const auto &child: expr->children) {
                if (!child || !isConstantExpression(child.get())) {
                    return false; // Non-constant argument found
                }
            }
            return true;
        }

        case ExprType::BINARY_OP: // Fallthrough to shared handling
        case ExprType::UNARY_OP: {
            // Operators (unary/binary) are constant if all children are constant
            for (const auto &child: expr->children) {
                if (!child || !isConstantExpression(child.get())) {
                    return false;
                }
            }
            return true;
        }

        default:
            return false; // Unknown expression types are treated as non-constant
    }
}


void LetCodeGenerator::emitBinaryOperation(const std::string &op,
                                           const asmjit::x86::Xmm &exprReg,
                                           const asmjit::x86::Xmm &lhsReg,
                                           const asmjit::x86::Xmm &rhsReg,
                                           [[maybe_unused]] Expression *lhsExpr,
                                           [[maybe_unused]] Expression *rhsExpr) {
    asmjit::x86::Assembler *assembler = nullptr; // Explicit initialization
    if (initialize_assembler(assembler)) {
        if (debug) std::cerr << "Failed to initialize assembler in emitBinaryOperation." << std::endl;
        return;
    }

    // Make sure lhsReg is properly set in exprReg
    assembler->movaps(exprReg, lhsReg); // Copy lhs to exprReg for the operation

    if (op == "+") {
        assembler->addsd(exprReg, rhsReg);
    } else if (op == "-") {
        assembler->subsd(exprReg, rhsReg);
    } else if (op == "*") {
        assembler->mulsd(exprReg, rhsReg);
    } else if (op == "/") {
        assembler->divsd(exprReg, rhsReg);
    } else if (op == "^") {
        emitExponentiation(exprReg, lhsReg, rhsReg); // Exponentiation logic
    } else {
        if (debug)
            std::cerr << "Unsupported binary operator: " << op
                    << " [exprReg: " << exprReg.id()
                    << ", lhsReg: " << lhsReg.id()
                    << ", rhsReg: " << rhsReg.id() << "]" << std::endl;
        SignalHandler::instance().raise(22);
    }
}


std::string LetCodeGenerator::expressionToString(const Expression *expr) {
    if (!expr) return "<null>";

    switch (expr->type) {
        case ExprType::LITERAL:
            return expr->value; // Literal value, e.g., "42.0"

        case ExprType::VARIABLE:
            return expr->value; // Variable name, e.g., "x"

        case ExprType::CONSTANT:
            return expr->value; // Constant name, e.g., "PI"

        case ExprType::UNARY_OP: {
            if (expr->children.empty() || !expr->children[0])
                return "<invalid unary operation>";

            // Unary operation, like negation
            std::ostringstream oss;
            oss << "(" << expr->value << " " << expressionToString(expr->children[0].get()) << ")";
            return oss.str();
        }

        case ExprType::BINARY_OP: {
            if (expr->children.size() < 2 || !expr->children[0] || !expr->children[1])
                return "<invalid binary operation>";

            // Binary operation, like addition
            std::ostringstream oss;
            oss << "(" << expressionToString(expr->children[0].get()) << " "
                    << expr->value << " "
                    << expressionToString(expr->children[1].get()) << ")";
            return oss.str();
        }

        case ExprType::FUNCTION: {
            if (expr->children.empty())
                return expr->value + "()"; // Handle function call with no arguments

            // Function call, like sin(x) or myFunc(a, b)
            std::ostringstream oss;
            oss << expr->value << "(";
            for (size_t i = 0; i < expr->children.size(); ++i) {
                if (expr->children[i]) {
                    oss << expressionToString(expr->children[i].get());
                    if (i < expr->children.size() - 1) oss << ", ";
                }
            }
            oss << ")";
            return oss.str();
        }

        default:
            if (debug) std::cerr << "Warning: Unknown expression type encountered in expressionToString." << std::endl;
            return "<unknown expression>";
    }
}


std::string LetCodeGenerator::getUniqueTempName(const Expression *expr) {
    if (!expr) {
        return "InvalidExpression"; // Null expressions can't have meaningful names
    }

    // If the name for this expression was already computed, return it
    auto it = expressionNameMap.find(expr);
    if (it != expressionNameMap.end()) {
        return it->second;
    }

    // Hash the pointer to generate a unique value
    auto hashValue = std::hash<const Expression *>()(expr);

    // Create a human-readable and unique name
    std::ostringstream nameStream;

    // Base the name on the Expression type
    switch (expr->type) {
        case ExprType::LITERAL:
            nameStream << "Const_" << expr->value; // Assuming Expression has a "value" field
            break;

        case ExprType::VARIABLE:
            nameStream << "Var_" << expr->value; // Assuming Expression has a "variableName" field
            break;

        case ExprType::FUNCTION:
            nameStream << "FuncCall_" << expr->value; // Assuming Expression has a "functionName" field
            break;

        case ExprType::BINARY_OP:
            nameStream << "BinaryOp_" << expr->value; // Assuming the operator is stored in `op`
            break;

        case ExprType::UNARY_OP:
            nameStream << "UnaryOp_" << expr->value; // Assuming the operator is stored in `op`
            break;

        default:
            nameStream << "Expr_UnknownType";
    }

    // Append hash in hexadecimal format for uniqueness
    nameStream << "_0x" << std::hex << hashValue;

    // Save the generated name in the map to avoid recomputation
    std::string uniqueName = nameStream.str();
    expressionNameMap[expr] = uniqueName;

    return uniqueName;
}


std::string LetCodeGenerator::expressionToText(const Expression *expr) {
    if (!expr) {
        return "<null>";
    }

    switch (expr->type) {
        case ExprType::LITERAL:
        case ExprType::CONSTANT:
        case ExprType::VARIABLE:
            // Base case: For constants and literals, return their value
            return expr->value;

        case ExprType::UNARY_OP: {
            return expr->value;
        }

        case ExprType::BINARY_OP: {
            // Recursive case: For binary operations like "+", "*", etc.
            if (expr->children.size() != 2) {
                return "<invalid-binary-operation>";
            }
            std::string left = expressionToText(expr->children[0].get());
            std::string right = expressionToText(expr->children[1].get());
            return "(" + left + " " + expr->value + " " + right + ")";
        }

        case ExprType::FUNCTION: {
            // Recursive case: For functions like "pow", "sqrt", etc.
            std::string args;
            for (size_t i = 0; i < expr->children.size(); ++i) {
                if (i > 0) args += ", "; // Add a comma for argument separation
                args += expressionToText(expr->children[i].get());
            }
            return expr->value + "(" + args + ")";
        }

        default:
            return "<unknown-expression-type>";
    }
}


void LetCodeGenerator::loadArguments(const std::vector<std::string> &params) {
    asmjit::x86::Assembler *assembler = nullptr;
    if (initialize_assembler(assembler)) {
        if (debug) std::cerr << "Failed to initialize assembler." << std::endl;
        return;
    }

    size_t numParams = params.size();
    if (numParams == 0) return; // No parameters, nothing to do.


    auto paramIt = params.rbegin();

    const auto &param1 = *paramIt++;
    if (debug) std::cerr << "LOAD_PARAM 1 " << param1 << "\n";
    auto reg1 = tracker.allocateRegister(param1);
    tracker.setConstant(param1);
    assembler->commentf("; Load 1st variable from FORTH stack (TOS): %s", param1.c_str());
    assembler->movq(reg1, asmjit::x86::r13);


    if (numParams >= 2) {
        const auto &param2 = *paramIt++;
        if (debug) std::cerr << "LOAD_PARAM 2 " << param2 << "\n";
        auto reg2 = tracker.allocateRegister(param2);
        tracker.setConstant(param2);
        assembler->commentf("; Load 2nd variable from FORTH stack (TOS-1): %s", param2.c_str());
        assembler->movq(reg2, asmjit::x86::r12);
    }


    size_t remaining = numParams > 2 ? numParams - 2 : 0;
    for (size_t i = 0; i < remaining; ++i, ++paramIt) {
        const auto &param = *paramIt;
        if (debug) std::cerr << "LOAD_PARAM 3+" << param << "\n";
        auto reg = tracker.allocateRegister(param);
        tracker.setConstant(param);
        assembler->commentf("; Load Nth variable from FORTH stack [R15+%d]: %s", i * 8, param.c_str());
        assembler->movq(reg, asmjit::x86::ptr(asmjit::x86::r15, i * 8));
    }


    assembler->comment("; -- FINAL STACK CORRECTION --");
    assembler->mov(asmjit::x86::r13, asmjit::x86::ptr(asmjit::x86::r15, (remaining) * 8)); // TOS from stack
    assembler->mov(asmjit::x86::r12, asmjit::x86::ptr(asmjit::x86::r15, (remaining + 1) * 8)); // TOS-1 from stack
    assembler->add(asmjit::x86::r15, numParams * 8);
}
