#include <ForthDictionary.h>

#include "ForthSystem.h"
#include "Quit.h"
#include <iostream>
#include "ParseLet.h"

void printAST(const ASTNode* root);



int main() {

    // std::string input = "LET (x, y) = FN(a, b) = a + b * sqrt(a) WHERE b = 2.0;";
    // auto tokens = tokenize(input);
    //
    // // Print out the tokens for demonstration
    // for (const auto &tk : tokens) {
    //     std::cout << "(" << tk.text << ", ";
    //     switch (tk.type) {
    //         case let_token_type::KEYWORD: std::cout << "KEYWORD"; break;
    //         case let_token_type::FUNC:    std::cout << "FUNC";    break;
    //         case let_token_type::OP:      std::cout << "OP";      break;
    //         case let_token_type::VAR:     std::cout << "VAR";     break;
    //         case let_token_type::NUM:     std::cout << "NUM";     break;
    //         case let_token_type::PAREN:   std::cout << "PAREN";   break;
    //         case let_token_type::DELIM:   std::cout << "DELIM";   break;
    //         default:                      std::cout << "UNKNOWN"; break;
    //     }
    //     std::cout << ")\n";
    // }
    //
    // //
    // Parser parser(tokens);
    // try {
    //     // Parse the tokens into a LetStatement AST
    //     auto ast = parser.parseLetStatement();
    //
    //     // Print the AST
    //     std::cout << "\nAbstract Syntax Tree (AST):\n";
    //     parser.printAST(ast.get());
    //
    // } catch (const std::exception &e) {
    //     std::cerr << "Error while parsing: " << e.what() << std::endl;
    //     return 1;
    // }

    ForthSystem::initialize();
    code_generator_initialize();
    Quit();
    return 0;


}
