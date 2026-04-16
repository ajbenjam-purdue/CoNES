#include "../src/lang/lexer.hpp"
#include <iostream>
#include <iomanip>

using namespace cones;

int main()
{
    std::string source = "T_out.guess := {350 : 273.15 : _} // Set guess and lower bound\n"
                         "P * V = n * R * T";

    Lexer lexer(source);
    std::vector<Token> tokens = lexer.scan_tokens();

    std::cout << "Lexing Result:" << std::endl;
    std::cout << std::left << std::setw(20) << "Type" << "Lexeme" << std::endl;
    std::cout << "-----------------------------------" << std::endl;

    for (const auto &token : tokens)
    {
        std::cout << std::left << std::setw(20) << token.type << "'" << token.lexeme << "'" << std::endl;
    }

    return 0;
}
