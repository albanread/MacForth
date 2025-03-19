#ifndef PARSELET_H
#define PARSELET_H
#include "SignalHandler.h"
#include <string>
#include <vector>
#include <set>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include "LexLet.h" // The updated lexer header

// Base abstract node
struct ASTNode {
    virtual ~ASTNode() = default;
};

// Expression node types
enum class ExprType {
    LITERAL, // e.g., 42, "hello"
    CONSTANT, // e.g., a variable that's effectively constant, like PI
    VARIABLE, // dynamic variable
    FUNCTION, // sqrt(x), sin(x)
    BINARY_OP, // a + b, a * b
    UNARY_OP // -a, +a
};

// Expression node
struct Expression : public ASTNode {
    ExprType type;
    std::string value; // literal text, variable name, function name, or operator symbol
    std::vector<std::unique_ptr<Expression> > children;
    bool processed = false;
    bool isConstant = false;
    bool isEvaluated = false;
    double evaluatedValue = 0;


    Expression(ExprType t, const std::string &val)
        : type(t), value(val) {

    }

    [[nodiscard]] bool hasPrecomputedValue() const {
        return isEvaluated;
    }

    double getPrecomputedValue() const {
        if (!hasPrecomputedValue()) {
            std::cerr << "no precomputed value" << std::endl;
            SignalHandler::instance().raise(24); // Syntax error
            return 0;
        }
        return evaluatedValue;
    }

    void setPrecomputedValue(double val) {
        evaluatedValue = val;
        isConstant = true;
        isEvaluated = true;
    }

};

// WHERE clause (var = expr)
struct WhereClause : public ASTNode {
    std::string varName;
    std::unique_ptr<Expression> expr;
};

// LET statement
struct LetStatement : public ASTNode {
    std::vector<std::string> outputVars; // from (x, y, z)
    std::vector<std::string> inputParams; // from FN(a, b, c)
    std::vector<std::unique_ptr<Expression> > expressions;
    std::vector<std::unique_ptr<WhereClause> > whereClauses;
};

//-------------------- Helper Functions -----------------------//

// Collect variables from an expression
inline void collectVariables(const Expression *expr, std::set<std::string> &vars) {
    if (!expr) return;
    if (expr->type == ExprType::VARIABLE) {
        vars.insert(expr->value);
    }
    // Recurse on children
    for (auto &child: expr->children) {
        collectVariables(child.get(), vars);
    }
}

inline void validateVariableReferences(const LetStatement *letStmt) {
    if (!letStmt) return;

    // Step 1: Collect all known variables (outputVars, inputParams, WHERE clause variables)
    std::unordered_set<std::string> validVariables;
    validVariables.insert(letStmt->outputVars.begin(), letStmt->outputVars.end());
    validVariables.insert(letStmt->inputParams.begin(), letStmt->inputParams.end());

    for (const auto &wc: letStmt->whereClauses) {
        validVariables.insert(wc->varName);
    }

    // Step 2: Traverse all expressions and collect referenced variables
    std::set<std::string> usedVariables; // Track variables used in expressions/WHERE clauses
    for (const auto &expr: letStmt->expressions) {
        collectVariables(expr.get(), usedVariables); // Collect variables from expressions
    }
    for (const auto &wc: letStmt->whereClauses) {
        collectVariables(wc->expr.get(), usedVariables); // Collect variables from WHERE clause expressions
    }

    // Step 3: Check that all used variables are valid
    for (const auto &var: usedVariables) {
        if (validVariables.count(var) == 0) {
            std::cerr << "Error: Undefined variable '" << var << "' used in LET statement.\n";
            SignalHandler::instance().raise(24); // Syntax error
        }
    }
}



inline bool isConstantExpression(const Expression *expr,
                                 const std::unordered_set<std::string> &knownConstants) {
    if (!expr) return false;

    // LITERALs are always constant
    if (expr->type == ExprType::LITERAL) return true;

    // VARIABLEs must exist in the known constants set
    if (expr->type == ExprType::VARIABLE) {
        return knownConstants.count(expr->value) > 0;
    }

    // Check all children for FUNCTION, BINARY_OP, UNARY_OP
    if (expr->type == ExprType::FUNCTION ||
        expr->type == ExprType::BINARY_OP ||
        expr->type == ExprType::UNARY_OP) {
        for (const auto &child: expr->children) {
            if (!isConstantExpression(child.get(), knownConstants)) {
                return false;
            }
        }
        return true;
    }

    return false;
}


// Detect circular dependencies in WHERE clauses
inline void detectCircularDependency(const std::vector<std::vector<int> > &dependencies) {
    std::vector<bool> visited(dependencies.size(), false);
    std::vector<bool> stack(dependencies.size(), false);

    std::function<bool(int)> hasCycle = [&](int node) -> bool {
        if (!visited[node]) {
            visited[node] = true;
            stack[node] = true;
            for (int neighbor: dependencies[node]) {
                if (!visited[neighbor] && hasCycle(neighbor)) return true;
                if (stack[neighbor]) return true;
            }
        }
        stack[node] = false;
        return false;
    };

    for (size_t i = 0; i < dependencies.size(); ++i) {
        if (hasCycle(static_cast<int>(i))) {
            std::cerr << ("Error: Circular dependency detected in WHERE clauses.") << std::endl;
            std::cerr << "  Node " << i << " depends on itself." << std::endl;
            std::cerr << "  Dependencies: ";
            for (int neighbor: dependencies[i]) {
                std::cerr << neighbor << " ";
            }
            std::cerr << std::endl;
            SignalHandler::instance().raise(24);
        }
    }
}

inline void printExpression(const Expression *expr, int indent = 0) {
    if (!expr) return;
    std::string indentStr(indent * 2, ' ');
    if (expr->type == ExprType::LITERAL) {
        std::cout << indentStr << expr->value << "\n";
    } else if (expr->type == ExprType::VARIABLE) {
        std::cout << indentStr << expr->value << "\n";
    } else if (expr->type == ExprType::CONSTANT) {
        std::cout << indentStr << expr->value << "\n";
    } else if (expr->type == ExprType::FUNCTION) {
        std::cout << indentStr << expr->value << "(";
        for (size_t i = 0; i < expr->children.size(); ++i) {
        }
    }
}

inline void printWhereClause(const WhereClause *wc, int indent = 0) {
    std::string indentStr(indent * 2, ' ');
    std::cout << indentStr << "Where: " << wc->varName << " =\n";
    printExpression(wc->expr.get(), indent + 1);
}


//-------------------- Parser Class -----------------------//

class Parser {
public:
    explicit Parser(const std::vector<let_token> &letTokens)
        : tokens_(letTokens), pos_(0) {
        if (tokens_.empty()) {
            std::cerr << ("Error: Empty token list provided.") << std::endl;
            SignalHandler::instance().raise(24);
        }
    }


    void propagateConstants(Expression *expr, const std::unordered_set<std::string> &knownConstants) {
        if (!expr) return;

        // Step 1: Determine if the current node is constant
        if (expr->type == ExprType::VARIABLE && knownConstants.count(expr->value) > 0) {
            // Reclassify this VARIABLE as a CONSTANT
            expr->type = ExprType::CONSTANT;
            expr->isConstant = true;
        } else if (expr->type == ExprType::LITERAL) {
            // Literals are always constant
            expr->isConstant = true;
        } else if (expr->type == ExprType::CONSTANT) {
            // Already marked as constant, nothing more to do
            expr->isConstant = true;
        }

        // Step 2: Recursively propagate constants for child nodes in FUNCTIONS, OPERATORS, etc.
        bool allChildrenAreConstant = true; // to determine if parents can also be constant
        for (auto &child: expr->children) {
            propagateConstants(child.get(), knownConstants);
            if (child != nullptr && !child->isConstant) {
                allChildrenAreConstant = false;
            }
        }

        // Step 3: Update this node's constant status based on its type and children
        if (expr->type == ExprType::FUNCTION || expr->type == ExprType::BINARY_OP || expr->type == ExprType::UNARY_OP) {
            expr->isConstant = allChildrenAreConstant;
        }
    }

    bool isLiteralExpression(const Expression *expr) {
        if (!expr) return false;

        // Directly check if the expression type is a literal
        return expr->type == ExprType::LITERAL;
    }

    std::unique_ptr<LetStatement> parseLetStatement() {
        expectKeyword("LET");
        auto outVars = parseParenVarList();

        expectOperator("=");
        expectKeyword("FN");
        auto inParams = parseParenVarList();

        // Mark input parameters as constants
        for (const auto &param: inParams) {
            knownConstants.insert(param);
        }

        expectOperator("=");
        auto exprs = parseExpressionList(); // possibly multiple expressions separated by commas

        // Parse all WHERE clauses initially
        std::vector<std::unique_ptr<WhereClause> > whereClauses;
        while (matchKeyword("WHERE")) {
            auto wc = std::make_unique<WhereClause>();
            wc->varName = expectVar();
            expectOperator("=");
            wc->expr = parseExpression();
            whereClauses.push_back(std::move(wc));
        }

        // Process WHERE clauses to determine constants and detect reassignment
        for (const auto &wc: whereClauses) {
            // If the variable is already a known constant, raise an error
            if (knownConstants.count(wc->varName) > 0) {
                std::cerr << "Error: Attempting to reassign a value to constant '"
                        << wc->varName << "' in WHERE clause.\n";
                SignalHandler::instance().raise(24); // Raise syntax error
            }

            // If the WHERE clause defines a constant (e.g., `pi = 3.14`),
            // add it to the known constants
            if (isLiteralExpression(wc->expr.get())) {
                // Helper function
                knownConstants.insert(wc->varName);
            }
        }

        // Sort WHERE clauses by dependency order
        whereClauses = sortWhereClausesByDependency(std::move(whereClauses));

        // Expect final semicolon
        matchDelimiter(";");

        // Validate # of outputs vs # of expressions
        if (outVars.size() != exprs.size()) {
            std::cerr << (
                        "Mismatch: # of output variables (" + std::to_string(outVars.size()) +
                        ") != # of top-level expressions (" + std::to_string(exprs.size()) + ")")
                    << std::endl;
            SignalHandler::instance().raise(24);
        }

        // Build the final AST
        auto letStmt = std::make_unique<LetStatement>();
        letStmt->outputVars = std::move(outVars);
        letStmt->inputParams = std::move(inParams);
        letStmt->expressions = std::move(exprs);
        letStmt->whereClauses = std::move(whereClauses);

        // Propagate constants across all expressions and WHERE clause expressions
        for (auto &expr: letStmt->expressions) {
            propagateConstants(expr.get(), knownConstants);
        }

        for (auto &wc: letStmt->whereClauses) {
            propagateConstants(wc->expr.get(), knownConstants);
        }

        validateVariableReferences(letStmt.get());

        return letStmt;
    }


    // Print the resulting AST (for debugging/demo)
    void printAST(const ASTNode *root) {
        if (!root) return;
        if (const auto *ls = dynamic_cast<const LetStatement *>(root)) {
            printLetStatement(ls);
        } else {
            std::cerr << "Unknown AST node type encountered.\n";
            SignalHandler::instance().raise(24);
        }
    }

private:
    //=== Low-Level Accessors ===//
    const let_token &currentToken() const {
        if (pos_ >= tokens_.size()) {
            std::cerr << "Unexpected end of token stream." << std::endl;
            SignalHandler::instance().raise(24);
        }
        return tokens_[pos_];
    }

    bool isAtEnd() const {
        return pos_ >= tokens_.size();
    }

    const let_token &advance() {
        if (!isAtEnd()) pos_++;
        return previousToken();
    }

    const let_token &previousToken() const {
        return tokens_[pos_ - 1];
    }

    //=== Matching Helpers ===//
    bool matchKeyword(const std::string &kw) {
        if (!isAtEnd() &&
            currentToken().type == let_token_type::KEYWORD &&
            toUpper(currentToken().text) == kw) {
            advance();
            return true;
        }
        return false;
    }

    void expectKeyword(const std::string &kw) {
        if (!matchKeyword(kw)) {
            error("Expected keyword: " + kw);
        }
    }

    bool matchOperator(const std::string &op) {
        if (!isAtEnd() &&
            currentToken().type == let_token_type::OP &&
            currentToken().text == op) {
            advance();
            return true;
        }
        return false;
    }

    void expectOperator(const std::string &op) {
        if (!matchOperator(op)) {
            error("Expected operator: " + op);
        }
    }

    bool matchDelimiter(const std::string &delim) {
        if (!isAtEnd() &&
            currentToken().type == let_token_type::DELIM &&
            currentToken().text == delim) {
            advance();
            return true;
        }
        return false;
    }

    //=== Error Utility ===//
    void error(const std::string &msg) const {
        std::string positionInfo;
        if (!isAtEnd()) {
            positionInfo = " (at token text='" + currentToken().text +
                           "', pos=" + std::to_string(currentToken().position) + ")";
        }
        std::cerr << "Error: " << msg << positionInfo << std::endl;
        SignalHandler::instance().raise(24);
    }

    //=== Variable Expectation ===//
    std::string expectVar() {
        if (isAtEnd()) {
            error("Expected variable name, but reached end of tokens");
        }
        if (currentToken().type != let_token_type::VAR) {
            error("Expected variable name, found '" + currentToken().text + "'");
        }
        std::string v = currentToken().text;
        advance();
        return v;
    }

    //=== Parse (x, y, z) ===//
    std::vector<std::string> parseParenVarList() {
        // Expect '('
        if (isAtEnd() || currentToken().text != "(") {
            error("Expected '('");
        }
        advance(); // consume '('

        std::vector<std::string> vars;
        // if next is ')', empty list
        if (!isAtEnd() && currentToken().text != ")") {
            vars.push_back(expectVar());
            while (matchDelimiter(",")) {
                vars.push_back(expectVar());
            }
        }

        // expect ')'
        if (isAtEnd() || currentToken().text != ")") {
            error("Expected ')'");
        }
        advance(); // consume ')'

        return vars;
    }

    //=== Parse Expression List: expr, expr, expr ===//
    std::vector<std::unique_ptr<Expression> > parseExpressionList() {
        std::vector<std::unique_ptr<Expression> > exprs;
        exprs.push_back(parseExpression());
        while (matchDelimiter(",")) {
            exprs.push_back(parseExpression());
        }
        return exprs;
    }

    //=== Expression Parsing ===//
    // We handle standard precedence: unary -> power -> mul/div -> add/sub

    std::unique_ptr<Expression> parseExpression() {
        return parseAddSub();
    }

    // parseAddSub: left-associative
    std::unique_ptr<Expression> parseAddSub() {
        auto left = parseMulDiv();
        while (!isAtEnd() && (currentToken().text == "+" || currentToken().text == "-")) {
            std::string op = currentToken().text;
            advance();
            auto right = parseMulDiv();

            auto expr = std::make_unique<Expression>(ExprType::BINARY_OP, op);
            expr->children.push_back(std::move(left));
            expr->children.push_back(std::move(right));

            if (expr->children[0]->isConstant && expr->children[1]->isConstant) {
                expr->isConstant = true;
            }

            left = std::move(expr);
        }
        return left;
    }

    // parseMulDiv: left-associative
    std::unique_ptr<Expression> parseMulDiv() {
        auto left = parsePower();
        while (!isAtEnd() && (currentToken().text == "*" || currentToken().text == "/")) {
            std::string op = currentToken().text;
            advance();
            auto right = parsePower();

            auto expr = std::make_unique<Expression>(ExprType::BINARY_OP, op);
            expr->children.push_back(std::move(left));
            expr->children.push_back(std::move(right));

            if (expr->children[0]->isConstant && expr->children[1]->isConstant) {
                expr->isConstant = true;
            }


            left = std::move(expr);
        }
        return left;
    }

    // parsePower: handle '^' right-associative
    std::unique_ptr<Expression> parsePower() {
        auto left = parseUnary();
        if (!isAtEnd() && currentToken().text == "^") {
            std::string op = currentToken().text;
            advance();
            auto right = parsePower(); // right-associative
            auto expr = std::make_unique<Expression>(ExprType::BINARY_OP, op);
            expr->children.push_back(std::move(left));
            expr->children.push_back(std::move(right));

            if (expr->children[0]->isConstant && expr->children[1]->isConstant) {
                expr->isConstant = true;
            }

            return expr;
        }
        return left;
    }

    // parseUnary: handle unary minus
    std::unique_ptr<Expression> parseUnary() {
        // unary minus
        if (!isAtEnd() && currentToken().text == "-") {
            advance(); // consume '-'
            auto child = parseUnary(); // parse factor after minus
            auto expr = std::make_unique<Expression>(ExprType::UNARY_OP, "neg");
            expr->children.push_back(std::move(child));

            if (expr->children[0]->isConstant && expr->children[1]->isConstant) {
                expr->isConstant = true;
            }


            return expr;
        }
        return parseFactor();
    }

    // parseFactor: literal, variable, function call, or parenthesized expr
    std::unique_ptr<Expression> parseFactor() {
        if (isAtEnd()) {
            error("Unexpected end of token stream while parsing factor");
        }

        // Grouped expression
        if (currentToken().text == "(") {
            advance(); // consume '('
            auto expr = parseExpression();
            if (isAtEnd() || currentToken().text != ")") {
                error("Expected ')' to close grouped expression");
            }
            advance(); // consume ')'
            return expr;
        }

        // Numeric literal
        if (currentToken().type == let_token_type::NUM) {
            auto expr = std::make_unique<Expression>(ExprType::LITERAL, currentToken().text);
            expr->isConstant = true;
            advance();
            return expr;;
        }

        // Variable
        if (currentToken().type == let_token_type::VAR) {
            bool isConst = knownConstants.find(currentToken().text) != knownConstants.end();
            auto exprType = isConst ? ExprType::CONSTANT : ExprType::VARIABLE;
            auto expr = std::make_unique<Expression>(exprType, currentToken().text);
            expr->isConstant = isConst;
            advance();
            return expr;
        }

        // Function call: e.g. sqrt(...), sin(...), possibly multi-arg
        if (currentToken().type == let_token_type::FUNC) {
            std::string funcName = currentToken().text;
            advance(); // consume function token

            if (isAtEnd() || currentToken().text != "(") {
                error("Expected '(' after function name '" + funcName + "'");
            }
            advance(); // consume '('

            auto funcExpr = std::make_unique<Expression>(ExprType::FUNCTION, funcName);

            // Parse at least one argument unless we see ')'
            if (!isAtEnd() && currentToken().text != ")") {
                funcExpr->children.push_back(parseExpression());
                // Possibly more comma-separated arguments
                while (matchDelimiter(",")) {
                    funcExpr->children.push_back(parseExpression());
                }
            }

            if (isAtEnd() || currentToken().text != ")") {
                error("Expected ')' to close function arguments for '" + funcName + "'");
            }
            advance(); // consume ')'

            return funcExpr;
        }

        // If UNKNOWN or something else, error
        error("Unexpected token while parsing factor: '" + currentToken().text + "'");
        return nullptr; // unreachable
    }

    //=== Debug Printing ===//

    static std::string printExprType(const ExprType type) {
        switch (type) {
            case ExprType::LITERAL:
                return "Literal";
            case ExprType::CONSTANT:
                return "Constant";
            case ExprType::VARIABLE:
                return "Variable";
            case ExprType::FUNCTION:
                return "Function";
            case ExprType::BINARY_OP:
                return "Binary Operation";
            case ExprType::UNARY_OP:
                return "Unary Operation";
            default:
                return "Unknown Expression Type";
        }
    }

    void printExpression(const Expression *expr, int indent = 0) const {
        if (!expr) return;
        std::string indentStr(indent * 2, ' ');
        std::cout << indentStr << "ExprType=" << printExprType(expr->type)
                << ",  value='" << expr->value << "'\n";
        if (expr->isConstant) std::cout << indentStr << " is Constant.\n";
        for (auto &child: expr->children) {
            printExpression(child.get(), indent + 1);
        }
    }

    void printWhereClause(const WhereClause *wc, int indent = 0) const {
        std::string indentStr(indent * 2, ' ');
        std::cout << indentStr << "Where: " << wc->varName << " =\n";
        printExpression(wc->expr.get(), indent + 1);
    }

    void printLetStatement(const LetStatement *letStmt) const {
        std::cout << "LetStatement:\n";
        std::cout << "  Output Vars: ";
        for (auto &ov: letStmt->outputVars) std::cout << ov << " ";
        std::cout << "\n  Input Params: ";
        for (auto &ip: letStmt->inputParams) std::cout << ip << " ";
        std::cout << "\n  Expressions:\n";
        for (auto &ex: letStmt->expressions) {
            printExpression(ex.get(), 2);
        }
        std::cout << "  Where Clauses:\n";
        for (auto &wc: letStmt->whereClauses) {
            printWhereClause(wc.get(), 2);
        }

        // print Known constants
        std::cout << "  Known Constants: ";
        for (auto &c: knownConstants) std::cout << c << " ";
        std::cout << "\n";
    }

    std::vector<std::unique_ptr<WhereClause> > sortWhereClausesByDependency(
        std::vector<std::unique_ptr<WhereClause> > whereClauses) {
        // Map each variable name to its corresponding WHERE clause index
        std::unordered_map<std::string, int> varToClauseIndex;
        varToClauseIndex.reserve(whereClauses.size());
        for (int i = 0; i < static_cast<int>(whereClauses.size()); ++i) {
            varToClauseIndex[whereClauses[i]->varName] = i;
        }

        // Track dependencies between WHERE clauses
        std::vector<std::vector<int> > dependencies(whereClauses.size());

        // Analyze dependencies and detect constants
        for (int i = 0; i < static_cast<int>(whereClauses.size()); ++i) {
            std::set<std::string> usedVars;
            collectVariables(whereClauses[i]->expr.get(), usedVars);

            bool isConstant = true; // Determine if the current clause defines a constant
            for (const auto &v: usedVars) {
                auto it = varToClauseIndex.find(v);
                if (it != varToClauseIndex.end()) {
                    dependencies[i].push_back(it->second); // Track dependency
                    isConstant = false; // If it depends on another clause, it's not constant
                } else if (knownConstants.find(v) == knownConstants.end()) {
                    isConstant = false; // If it depends on something unknown, it's not constant
                }
            }

            // If determined to be a constant, add the variable to knownConstants
            if (isConstant) {
                knownConstants.insert(whereClauses[i]->varName);
            }
        }

        // Check for circular dependencies among non-constant clauses
        detectCircularDependency(dependencies);

        // Topological sorting of the WHERE clauses
        std::vector<int> sortedIndices;
        std::vector<bool> visited(whereClauses.size(), false);
        std::function<void(int)> dfs = [&](int node) {
            if (visited[node]) return;
            visited[node] = true;
            for (int neighbor: dependencies[node]) {
                dfs(neighbor);
            }
            sortedIndices.push_back(node);
        };
        for (size_t i = 0; i < whereClauses.size(); ++i) {
            if (!visited[i]) {
                dfs(static_cast<int>(i));
            }
        }
        std::reverse(sortedIndices.begin(), sortedIndices.end());

        // Rebuild the sorted WHERE clauses vector
        std::vector<std::unique_ptr<WhereClause> > sortedWhereClauses;
        for (auto it = sortedIndices.rbegin(); it != sortedIndices.rend(); ++it) {
            sortedWhereClauses.push_back(std::move(whereClauses[*it]));
        }

        return sortedWhereClauses;
    }

private:
    std::vector<let_token> tokens_;
    std::size_t pos_;
    std::unordered_set<std::string> knownConstants;
};

#endif // PARSELET_H
