#ifndef CONES_LANG_LEXER_HPP
#define CONES_LANG_LEXER_HPP

#include "token.hpp"
#include <vector>
#include <string>
#include <cctype>

namespace cones
{

    class Lexer
    {
        std::string source_;
        int start_ = 0;
        int current_ = 0;
        int line_ = 1;

    public:
        explicit Lexer(std::string source) : source_(std::move(source)) {}

        /**
         * @brief Scans the entire source string and returns a vector of tokens.
         */
        std::vector<Token> scan_tokens()
        {
            std::vector<Token> tokens;
            while (!is_at_end())
            {
                start_ = current_;
                scan_token(tokens);
            }
            tokens.emplace_back(TokenType::END_OF_FILE, "", line_);
            return tokens;
        }

    private:
        void scan_token(std::vector<Token> &tokens)
        {
            char c = advance();
            switch (c)
            {
            case '(':
                add_token(tokens, TokenType::LPAREN);
                break;
            case ')':
                add_token(tokens, TokenType::RPAREN);
                break;
            case '{':
                add_token(tokens, TokenType::LBRACE);
                break;
            case '}':
                add_token(tokens, TokenType::RBRACE);
                break;
            case '[': 
                add_token(tokens, TokenType::LBRACKET); 
                // Scan inner unit content as a special case or just allow IDs to handle it
                break;

            case ']':
                add_token(tokens, TokenType::RBRACKET);
                break;
            case ':': // either explicit def OR a separator
                if (match('='))
                    add_token(tokens, TokenType::COLON_EQUALS);
                else
                    add_token(tokens, TokenType::COLON);
                break;
            case ',':
                add_token(tokens, TokenType::COMMA);
                break;
            case '.':
                add_token(tokens, TokenType::DOT);
                break;
            case '+':
                add_token(tokens, TokenType::PLUS);
                break;
            case '-':
                add_token(tokens, TokenType::MINUS);
                break;
            case '*':
                add_token(tokens, TokenType::STAR);
                break;
            case '/':
                if (match('/'))
                { // Single-line comment
                    while (peek() != '\n' && !is_at_end())
                        advance();
                }
                else if (match('*'))
                { // Multi-line comment
                    while (!(peek() == '*' && peek_next() == '/') && !is_at_end())
                    {
                        if (peek() == '\n')
                            line_++;
                        advance();
                    }

                    if (!is_at_end())
                    {
                        advance(); // consume '*'
                        advance(); // consume '/'
                    }
                }
                else
                {
                    add_token(tokens, TokenType::SLASH);
                }
                break;
            case '^':
                add_token(tokens, TokenType::CARET);
                break;
            case '=':
                add_token(tokens, TokenType::EQUALS);
                break;
            case '_':
                add_token(tokens, TokenType::UNDERSCORE);
                break;

            case '"':
                string(tokens);
                break;

            case ' ':
            case '\r':
            case '\t':
                break; // Ignore whitespace
            case '\n':
                line_++;
                break;

            default:
                if (std::isdigit(c))
                {
                    number(tokens);
                }
                else if (std::isalpha(c) || c == '_')
                {
                    identifier(tokens);
                }
                else
                {
                    // Unknown character (could throw an error here)
                    add_token(tokens, TokenType::UNKNOWN);
                }
                break;
            }
        }

        void identifier(std::vector<Token> &tokens)
        {
            while (std::isalnum(peek()) || peek() == '_')
                advance();
            
            std::string text = source_.substr(start_, current_ - start_);
            if (text == "include") add_token(tokens, TokenType::INCLUDE);
            else add_token(tokens, TokenType::IDENTIFIER);
        }

        void string(std::vector<Token> &tokens)
        {
            while (peek() != '"' && !is_at_end())
            {
                if (peek() == '\n') line_++;
                advance();
            }

            if (is_at_end()) {
                add_token(tokens, TokenType::UNKNOWN);
                return;
            }

            advance(); // The closing ".

            // Trim the surrounding quotes
            std::string value = source_.substr(start_ + 1, current_ - start_ - 2);
            tokens.emplace_back(TokenType::STRING, value, line_);
        }

        void number(std::vector<Token> &tokens)
        {
            while (std::isdigit(peek()))
                advance();

            // Fraction
            if (peek() == '.' && std::isdigit(peek_next()))
            {
                advance(); // consume the '.'
                while (std::isdigit(peek()))
                    advance();
            }

            // Scientific notation
            if (peek() == 'e' || peek() == 'E')
            {
                advance(); // consume 'e'
                if (peek() == '+' || peek() == '-')
                    advance();
                while (std::isdigit(peek()))
                    advance();
            }

            add_token(tokens, TokenType::NUMBER);
        }

        // Helper methods
        bool is_at_end() const { return current_ >= (int)source_.length(); }
        char advance() { return source_[current_++]; }
        bool match(char expected)
        {
            if (is_at_end())
                return false;
            if (source_[current_] != expected)
                return false;
            current_++;
            return true;
        }
        char peek() const { return is_at_end() ? '\0' : source_[current_]; }
        char peek_next() const
        {
            if (current_ + 1 >= (int)source_.length())
                return '\0';
            return source_[current_ + 1];
        }

        void add_token(std::vector<Token> &tokens, TokenType type)
        {
            std::string text = source_.substr(start_, current_ - start_);
            tokens.emplace_back(type, text, line_);
        }
    };

} // namespace cones

#endif
