#ifndef CONES_LANG_TOKEN_HPP
#define CONES_LANG_TOKEN_HPP

#include <string>
#include <ostream>

namespace cones
{

    enum class TokenType
    {
        IDENTIFIER,
        NUMBER,
        STRING,

        // Operators
        PLUS,   // +
        MINUS,  // -
        STAR,   // *
        SLASH,  // /
        CARET,  // ^
        EQUALS, // =
        COLON_EQUALS,   // :=

        // Punctuation
        LPAREN,     // (
        RPAREN,     // )
        LBRACE,     // {
        RBRACE,     // }
        LBRACKET,   // [
        RBRACKET,   // ]
        COLON,  // :
        COMMA,  // ,
        DOT,    // .

        // Special
        UNDERSCORE, // _
        INCLUDE,    // include
        ROUTINE,    // routine
        FUNCTION,   // function
        RETURN,     // return
        END,        // end

        // End of file
        END_OF_FILE, // \0

        // Uh oh
        UNKNOWN // ?!
    };

    struct Token
    {
        TokenType type;
        std::string lexeme;
        int line;

        Token(TokenType t, std::string l, int ln) : type(t), lexeme(std::move(l)), line(ln) {}
    };

    // For debugging
    inline std::ostream &operator<<(std::ostream &os, const TokenType &type)
    {
        switch (type)
        {
        case TokenType::IDENTIFIER:
            os << "IDENTIFIER";
            break;
        case TokenType::NUMBER:
            os << "NUMBER";
            break;
        case TokenType::STRING:
            os << "STRING";
            break;
        case TokenType::PLUS:
            os << "PLUS";
            break;
        case TokenType::MINUS:
            os << "MINUS";
            break;
        case TokenType::STAR:
            os << "STAR";
            break;
        case TokenType::SLASH:
            os << "SLASH";
            break;
        case TokenType::CARET:
            os << "CARET";
            break;
        case TokenType::EQUALS:
            os << "EQUALS";
            break;
        case TokenType::COLON_EQUALS:
            os << "COLON_EQUALS";
            break;
        case TokenType::LPAREN:
            os << "LPAREN";
            break;
        case TokenType::RPAREN:
            os << "RPAREN";
            break;
        case TokenType::LBRACE:
            os << "LBRACE";
            break;
        case TokenType::RBRACE:
            os << "RBRACE";
            break;
        case TokenType::LBRACKET:
            os << "LBRACKET";
            break;
        case TokenType::RBRACKET:
            os << "RBRACKET";
            break;
        case TokenType::COLON:
            os << "COLON";
            break;
        case TokenType::COMMA:
            os << "COMMA";
            break;
        case TokenType::DOT:
            os << "DOT";
            break;
        case TokenType::UNDERSCORE:
            os << "UNDERSCORE";
            break;
        case TokenType::INCLUDE:
            os << "INCLUDE";
            break;
        case TokenType::ROUTINE:
            os << "ROUTINE";
            break;
        case TokenType::FUNCTION:
            os << "FUNCTION";
            break;
        case TokenType::RETURN:
            os << "RETURN";
            break;
        case TokenType::END:
            os << "END";
            break;
        case TokenType::END_OF_FILE:
            os << "EOF";
            break;
        default:
            os << "UNKNOWN";
            break;
        }
        return os;
    }

} // namespace cones

#endif
