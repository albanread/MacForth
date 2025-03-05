#include "LexLet.h"
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <functional>

// Base abstract node
struct ASTNode {
    virtual ~ASTNode() = default;
};

// Expression node types
enum class ExprType {
    LITERAL, // numeric constant
    VARIABLE, // single variable name
    FUNCTION, // sqrt(...), sin(...), etc.
    BINARY_OP, // a + b, a * b, etc.
    UNARY_OP // unary minus, etc.
};

// Expression node
struct Expression : public ASTNode {
    ExprType type;
    std::string value; // literal text, variable name, function name, or operator symbol
    std::vector<std::unique_ptr<Expression> > children;

    Expression(ExprType t, const std::string &val)
        : type(t), value(val) {
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
    std::vector<std::unique_ptr<Expression> > expressions; // after = ..., possibly multiple comma-separated
    std::vector<std::unique_ptr<WhereClause> > whereClauses;
};

// Detect circular dependencies in WHERE clauses
inline void detectCircularDependency(const std::vector<std::vector<int> > &dependencies) {
    std::vector<bool> visited(dependencies.size(), false);
    std::vector<bool> stack(dependencies.size(), false);

    std::function<bool(int)> hasCycle = [&](int node) {
        if (!visited[node]) {
            visited[node] = stack[node] = true;

            for (int neighbor: dependencies[node]) {
                if (!visited[neighbor] && hasCycle(neighbor)) return true;
                if (stack[neighbor]) return true;
            }
        }
        stack[node] = false;
        return false;
    };

    for (size_t i = 0; i < dependencies.size(); ++i) {
        if (hasCycle(i)) {
            throw std::runtime_error("Error: Circular dependency detected in WHERE clauses.");
        }
    }
}


// Collect variables from an expression and add them to a set
void collectVariables(const Expression *expr, std::set<std::string> &vars) {
    if (!expr) return;

    if (expr->type == ExprType::VARIABLE) {
        vars.insert(expr->value);
    }

    // Recurse on children
    for (auto &child: expr->children) {
        collectVariables(child.get(), vars);
    }
}

// Build WHERE clause dependencies and check for conflicts or circular dependencies
static std::vector<std::vector<int> > buildWhereDependencies(
    const std::vector<std::unique_ptr<WhereClause> > &whereClauses) {
    // Map each variable -> index of the clause that defines it
    std::unordered_map<std::string, int> varToClauseIndex;

    // Populate the map for quick lookup while ensuring no duplicate definitions
    for (int i = 0; i < static_cast<int>(whereClauses.size()); ++i) {
        const auto &wc = whereClauses[i];
        if (varToClauseIndex.find(wc->varName) != varToClauseIndex.end()) {
            throw std::runtime_error("Error: variable '" + wc->varName +
                                     "' defined more than once in WHERE clauses.");
        }
        varToClauseIndex[wc->varName] = i;
    }

    // Prepare adjacency list
    std::vector<std::vector<int> > dependencies(whereClauses.size());

    // Build dependencies by scanning each expression
    for (int i = 0; i < static_cast<int>(whereClauses.size()); ++i) {
        const auto &wc = whereClauses[i];
        std::set<std::string> usedVars;
        collectVariables(wc->expr.get(), usedVars);

        for (const auto &v: usedVars) {
            auto it = varToClauseIndex.find(v);
            if (it != varToClauseIndex.end()) {
                int definingClauseIndex = it->second;
                if (definingClauseIndex != i) {
                    dependencies[i].push_back(definingClauseIndex);
                }
            }
        }
    }
    // Detect circular dependencies
    detectCircularDependency(dependencies);
    return dependencies;
}

class Parser {
public:
    Parser(const std::vector<let_token> &let_tokens)
        : let_tokens_(let_tokens), pos_(0) {
        if (let_tokens_.empty()) {
            throw std::runtime_error("Error: Empty input provided to the parser.");
        }
    }

    std::unique_ptr<LetStatement> parseLetStatement() {
        expectKeyword("LET");
        auto outVars = parseParenVarList();
        expectOperator("=");
        expectKeyword("FN");
        auto inParams = parseParenVarList();
        expectOperator("=");
        auto expressions = parseExpressionList();

        std::vector<std::unique_ptr<WhereClause> > whereClauses;
        while (matchKeyword("WHERE")) {
            auto wc = std::make_unique<WhereClause>();
            wc->varName = expectVar();
            expectOperator("=");
            wc->expr = parseExpression();
            whereClauses.push_back(std::move(wc));
        }

        auto dependencies = buildWhereDependencies(whereClauses);
        matchDelimiter(";");

        auto letStmt = std::make_unique<LetStatement>();
        letStmt->outputVars = std::move(outVars);
        letStmt->inputParams = std::move(inParams);
        letStmt->expressions = std::move(expressions);
        letStmt->whereClauses = std::move(whereClauses);

        return letStmt;
    }

private:
    const let_token &currentlet_token() const {
        if (isAtEnd()) {
            throw std::runtime_error("Unexpected end of input.");
        }
        return let_tokens_[pos_];
    }

    bool isAtEnd() const { return pos_ >= let_tokens_.size(); }

    const let_token &advance() {
        if (!isAtEnd()) pos_++;
        return previouslet_token();
    }

    const let_token &previouslet_token() const { return let_tokens_[pos_ - 1]; }

    bool matchKeyword([[maybe_unused]] const std::string &kw) {
        static const std::set<std::string> keywords = {"LET", "FN", "WHERE"};
        if (currentlet_token().type == let_token_type::KEYWORD &&
            keywords.count(toUpper(currentlet_token().text))) {
            advance();
            return true;
        }
        return false;
    }

    void expectKeyword(const std::string &kw) {
        if (!matchKeyword(kw)) throw std::runtime_error("Expected keyword: " + kw);
    }

    bool matchOperator(const std::string &op) {
        if (currentlet_token().type == let_token_type::OP && currentlet_token().text == op) {
            advance();
            return true;
        }
        return false;
    }

    void expectOperator(const std::string &op) {
        if (!matchOperator(op)) throw std::runtime_error("Expected operator: " + op);
    }

    bool matchDelimiter(const std::string &delim) {
        if (currentlet_token().type == let_token_type::DELIM && currentlet_token().text == delim) {
            advance();
            return true;
        }
        return false;
    }

    std::string expectVar() {
        if (currentlet_token().type == let_token_type::VAR) {
            std::string v = currentlet_token().text;
            advance();
            return v;
        }
        throw std::runtime_error("Expected variable name at position " + std::to_string(pos_));
    }

    std::vector<std::string> parseParenVarList() {
        if (currentlet_token().text != "(") throw std::runtime_error("Expected '('");
        advance();

        std::vector<std::string> vars;
        if (currentlet_token().text != ")") {
            vars.push_back(expectVar());
            while (matchDelimiter(",")) vars.push_back(expectVar());
        }

        if (currentlet_token().text != ")") throw std::runtime_error("Expected ')'");
        advance();
        return vars;
    }

    std::vector<std::unique_ptr<Expression> > parseExpressionList() {
        std::vector<std::unique_ptr<Expression> > exprs;
        exprs.push_back(parseExpression());

        while (matchDelimiter(",")) {
            exprs.push_back(parseExpression());
        }
        return exprs;
    }

    std::unique_ptr<Expression> parseExpression() {
        // Start from the highest-level expression (Add/Sub)
        return parseAddSub();
    }

    std::unique_ptr<Expression> parseAddSub() {
        // Parse the left operand (higher precedence level: Mul/Div)
        auto left = parseMulDiv();

        // Parse any number of addition/subtraction operations
        while (!isAtEnd() && (currentlet_token().text == "+" || currentlet_token().text == "-")) {
            // Capture the operator
            std::string op = currentlet_token().text;
            advance();

            // Parse the right operand
            auto right = parseMulDiv();

            // Create a binary operation node
            auto expr = std::make_unique<Expression>(ExprType::BINARY_OP, op);
            expr->children.push_back(std::move(left));
            expr->children.push_back(std::move(right));

            // Update "left" to be this new expression
            left = std::move(expr);
        }

        return left;
    }

    std::unique_ptr<Expression> parseMulDiv() {
        // Parse the left operand (next precedence level: Power)
        auto left = parsePower();

        // Parse any number of multiplication/division operations
        while (!isAtEnd() && (currentlet_token().text == "*" || currentlet_token().text == "/")) {
            // Capture the operator
            std::string op = currentlet_token().text;
            advance();

            // Parse the right operand
            auto right = parsePower();

            // Create a binary operation node
            auto expr = std::make_unique<Expression>(ExprType::BINARY_OP, op);
            expr->children.push_back(std::move(left));
            expr->children.push_back(std::move(right));

            // Update "left" to be this new expression
            left = std::move(expr);
        }

        return left;
    }

    std::unique_ptr<Expression> parsePower() {
        // Parse the left operand (lower precedence level: Factor)
        auto left = parseFactor();

        // Handle right-associative power operator "^"
        if (!isAtEnd() && currentlet_token().text == "^") {
            // Capture the operator
            std::string op = currentlet_token().text;
            advance();

            // Parse the right operand
            auto right = parsePower();

            // Create a binary operation node
            auto expr = std::make_unique<Expression>(ExprType::BINARY_OP, op);
            expr->children.push_back(std::move(left));
            expr->children.push_back(std::move(right));

            return expr;
        }

        return left;
    }
std::unique_ptr<Expression> parseFactor() {
    // Handle grouped expressions "( ... )"
    if (currentlet_token().text == "(") {
        advance(); // Consume '('
        auto expr = parseExpression(); // Parse the inner expression
        if (currentlet_token().text != ")") {
            throw std::runtime_error("Expected ')' to close grouped expression");
        }
        advance(); // Consume ')'
        return expr;
    }

    // Handle numeric literals
    if (currentlet_token().type == let_token_type::NUM) {
        auto expr = std::make_unique<Expression>(ExprType::LITERAL, currentlet_token().text);
        advance();
        return expr;
    }

    // Handle variables
    if (currentlet_token().type == let_token_type::VAR) {
        auto expr = std::make_unique<Expression>(ExprType::VARIABLE, currentlet_token().text);
        advance();
        return expr;
    }

    // Handle function calls: FUNC "(" expression ")"
    if (currentlet_token().type == let_token_type::FUNC) {
        std::string funcName = currentlet_token().text; // Get the function name
        advance(); // Consume the FUNC token
        if (currentlet_token().text != "(") {
            throw std::runtime_error("Expected '(' after function name: " + funcName);
        }
        advance(); // Consume '('

        // Parse the argument(s) for the function
        auto funcExpr = std::make_unique<Expression>(ExprType::FUNCTION, funcName);
        funcExpr->children.push_back(parseExpression()); // Parse the argument

        if (currentlet_token().text != ")") {
            throw std::runtime_error("Expected ')' to close function arguments for: " + funcName);
        }
        advance(); // Consume ')'

        return funcExpr;
    }

    throw std::runtime_error("Unexpected token while parsing factor: " + currentlet_token().text);
}
    std::string toUpper(const std::string &s) {
        std::string result;
        result.reserve(s.size());
        for (char c: s) {
            result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
        return result;
    }



    // Function to print an Expression node
    void printExpression(const Expression *expr, int indent = 0) {
        if (!expr) return;

        // Print indentation
        for (int i = 0; i < indent; ++i) std::cout << "  ";

        // Print expression type and value
        std::cout << "Expression [" << static_cast<int>(expr->type) << "] : " << expr->value << std::endl;

        // Recursively print children
        for (const auto &child: expr->children) {
            printExpression(child.get(), indent + 1);
        }
    }

    // Function to print a WhereClause
    void printWhereClause(const WhereClause *whereClause, int indent = 0) {
        if (!whereClause) return;

        // Print indentation
        for (int i = 0; i < indent; ++i) std::cout << "  ";

        std::cout << "WhereClause: " << whereClause->varName << " = " << std::endl;

        // Print the corresponding expression
        printExpression(whereClause->expr.get(), indent + 1);
    }

    // Function to print a LetStatement
    void printLetStatement(const LetStatement *letStmt, int indent = 0) {
        if (!letStmt) return;

        // Print indentation
        for (int i = 0; i < indent; ++i) std::cout << "  ";

        std::cout << "LetStatement:" << std::endl;

        // Print output variables
        for (int i = 0; i < indent + 1; ++i) std::cout << "  ";
        std::cout << "Output Variables: ";
        for (const auto &var: letStmt->outputVars) {
            std::cout << var << " ";
        }
        std::cout << std::endl;

        // Print input parameters
        for (int i = 0; i < indent + 1; ++i) std::cout << "  ";
        std::cout << "Input Parameters: ";
        for (const auto &param: letStmt->inputParams) {
            std::cout << param << " ";
        }
        std::cout << std::endl;

        // Print expressions
        for (int i = 0; i < indent + 1; ++i) std::cout << "  ";
        std::cout << "Expressions:" << std::endl;
        for (const auto &expr: letStmt->expressions) {
            printExpression(expr.get(), indent + 2);
        }

        // Print WHERE clauses
        for (int i = 0; i < indent + 1; ++i) std::cout << "  ";
        std::cout << "Where Clauses:" << std::endl;
        for (const auto &whereClause: letStmt->whereClauses) {
            printWhereClause(whereClause.get(), indent + 2);
        }
    }

public:
    void printAST(const ASTNode *root) {
        // Helper function to start printing the ASTvoid printAST(const ASTNode* root) {
        if (!root) return;

        if (const auto *letStmt = dynamic_cast<const LetStatement *>(root)) {
            printLetStatement(letStmt);
        } else {
            std::cerr << "Unknown AST node type encountered!" << std::endl;
        }
    }

private:
    const std::vector<let_token> &let_tokens_;
    std::size_t pos_;
};
