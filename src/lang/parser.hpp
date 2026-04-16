#ifndef CONES_LANG_PARSER_HPP
#define CONES_LANG_PARSER_HPP

#include "lexer.hpp"
#include "../core/system.hpp"
#include <vector>
#include <stdexcept>
#include <string>

namespace cones {

/**
 * @brief Recursive Descent Parser for .cnes scripts.
 */
class Parser {
    std::vector<Token> tokens_;
    int current_ = 0;
    System& system_;

public:
    Parser(std::vector<Token> tokens, System& sys) 
        : tokens_(std::move(tokens)), system_(sys) {}

    void parse() {
        while (!is_at_end()) {
            statement();
        }
    }

private:
    // --- Grammar Rules ---

    void statement() {
        // Peek ahead to see if it's a Definition (:=) or Attribute Block ({) or Equation (=)
        if (check(TokenType::IDENTIFIER)) {
            if (peek_next().type == TokenType::COLON_EQUALS || 
                peek_next().type == TokenType::DOT ||
                peek_next().type == TokenType::LBRACKET ||
                peek_next().type == TokenType::LBRACE) { // Added LBRACE
                definition_statement();
                return;
            }
        }
        
        equation_statement();
    }

    /**
     * @brief Parses x := 5, x.lower := 0, x := {350 : 0 : 1000}
     */
    void definition_statement() {
        Token name = consume(TokenType::IDENTIFIER, "Expect variable name.");
        auto& reg = system_.registry();
        int idx = reg.register_variable(name.lexeme);

        if (match(TokenType::DOT)) {
            // ... (keep dot logic)
            Token attr = consume(TokenType::IDENTIFIER, "Expect attribute name (guess, lower, upper, unit).");
            consume(TokenType::COLON_EQUALS, "Expect ':=' after attribute.");
            
            if (attr.lexeme == "unit") {
                std::string unit_val = "";
                if (match(TokenType::LBRACKET)) {
                    while (!check(TokenType::RBRACKET) && !is_at_end()) {
                        unit_val += advance().lexeme;
                    }
                    consume(TokenType::RBRACKET, "Expect ']' after unit.");
                } else {
                    unit_val = consume(TokenType::IDENTIFIER, "Expect unit name.").lexeme;
                }
                reg.set_unit(idx, unit_val);
            } else {
                double val = evaluate_const_expression();
                if (attr.lexeme == "guess") reg.set_value(idx, val);
                else if (attr.lexeme == "lower") reg.set_bounds(idx, val, reg.get_variable(idx).upper_bound);
                else if (attr.lexeme == "upper") reg.set_bounds(idx, reg.get_variable(idx).lower_bound, val);
                else throw error(attr, "Unknown attribute: " + attr.lexeme);
            }
            
        } else if (match(TokenType::COLON_EQUALS) || check(TokenType::LBRACE)) {
            // x := 5 or x := { ... } or x { ... }
            bool has_assign = (previous().type == TokenType::COLON_EQUALS);
            
            if (match(TokenType::LBRACE)) {
                // ... (keep brace logic)
                if (!match(TokenType::UNDERSCORE)) reg.set_value(idx, evaluate_const_expression());
                consume(TokenType::COLON, "Expect ':' in attribute block.");
                if (!match(TokenType::UNDERSCORE)) {
                    double low = evaluate_const_expression();
                    reg.set_bounds(idx, low, reg.get_variable(idx).upper_bound);
                }
                consume(TokenType::COLON, "Expect ':' in attribute block.");
                if (!match(TokenType::UNDERSCORE)) {
                    double high = evaluate_const_expression();
                    reg.set_bounds(idx, reg.get_variable(idx).lower_bound, high);
                }
                consume(TokenType::RBRACE, "Expect '}' after attribute block.");
            } else if (has_assign) {
                // Simple fix: x := 300
                double val = evaluate_const_expression();
                reg.set_value(idx, val);
                reg.set_fixed(idx, true);
            }
        }

        // Optional units [m/s^2]
        if (match(TokenType::LBRACKET)) {
            std::string unit_content = "";
            while (!check(TokenType::RBRACKET) && !is_at_end()) {
                unit_content += advance().lexeme;
            }
            reg.set_unit(idx, unit_content);
            consume(TokenType::RBRACKET, "Expect ']' after unit.");
        }
    }

    void equation_statement() {
        NodePtr lhs = expression();
        consume(TokenType::EQUALS, "Expect '=' in equation.");
        NodePtr rhs = expression();
        
        // --- Unit Inheritance ---
        if (auto var_node = std::dynamic_pointer_cast<VariableNode>(lhs)) {
            auto& var_info = system_.registry().get_variable(system_.registry().get_index(var_node->to_string()));
            if (var_info.unit.is_dimensionless()) {
                Unit inherited = rhs->get_unit(system_.registry());
                system_.registry().set_unit(var_info.index, inherited, inherited.to_string());
            }
        }

        // Equation is f(x) = g(x) -> f(x) - g(x) = 0
        system_.add_equation(std::make_shared<SubNode>(lhs, rhs));
    }

    // --- Expression Parsing (Precedence Climbing) ---

    NodePtr expression() { return addition(); }

    NodePtr addition() {
        NodePtr node = multiplication();
        while (match(TokenType::PLUS, TokenType::MINUS)) {
            Token op = previous();
            NodePtr right = multiplication();
            if (op.type == TokenType::PLUS) node = std::make_shared<AddNode>(node, right);
            else node = std::make_shared<SubNode>(node, right);
        }
        return node;
    }

    NodePtr multiplication() {
        NodePtr node = unary();
        while (match(TokenType::STAR, TokenType::SLASH)) {
            Token op = previous();
            NodePtr right = unary();
            if (op.type == TokenType::STAR) node = std::make_shared<MulNode>(node, right);
            else node = std::make_shared<DivNode>(node, right);
        }
        return node;
    }

    NodePtr unary() {
        if (match(TokenType::MINUS)) {
            return std::make_shared<NegNode>(unary());
        }
        return power();
    }

    NodePtr power() {
        NodePtr node = primary();
        if (match(TokenType::CARET)) {
            // For now, PowNode takes a double exponent for simplicity in the AST
            // We'll evaluate the exponent as a constant
            double exp_val = evaluate_const_expression();
            node = std::make_shared<PowNode>(node, exp_val);
        }
        return node;
    }

    NodePtr primary() {
        NodePtr node = primary_base();

        // --- Unit Casting: Expression [Unit] ---
        if (match(TokenType::LBRACKET)) {
            std::string unit_name = "";
            while (!check(TokenType::RBRACKET) && !is_at_end()) {
                unit_name += advance().lexeme;
            }
            consume(TokenType::RBRACKET, "Expect ']' after unit cast.");
            
            const VariableRegistry& reg = system_.registry();
            Unit from = node->get_unit(reg);
            Unit to = Unit::from_string(unit_name);
            
            // If casting a single variable, update its registry unit too
            if (auto var_node = std::dynamic_pointer_cast<VariableNode>(node)) {
                 system_.registry().set_unit(system_.registry().get_index(var_node->to_string()), to, unit_name);
            }

            node = std::make_shared<UnitCastNode>(node, from, to);
        }

        return node;
    }

    NodePtr primary_base() {
        if (match(TokenType::NUMBER)) {
            return std::make_shared<ConstantNode>(std::stod(previous().lexeme));
        }

        if (match(TokenType::IDENTIFIER)) {
            Token name = previous();
            
            // Function call?
            if (match(TokenType::LPAREN)) {
                std::vector<NodeArg> args;
                if (!check(TokenType::RPAREN)) {
                    do {
                        std::string arg_name = "";
                        // Lookahead for name=expression
                        if (check(TokenType::IDENTIFIER) && peek_next().type == TokenType::EQUALS) {
                            arg_name = advance().lexeme;
                            consume(TokenType::EQUALS, "Expect '=' after argument name.");
                        }
                        args.push_back({arg_name, expression()});
                    } while (match(TokenType::COMMA));
                }
                consume(TokenType::RPAREN, "Expect ')' after function arguments.");
                
                // 1. Check built-ins
                if (args.size() == 1 && args[0].name.empty()) {
                    if (name.lexeme == "sin") return std::make_shared<SinNode>(args[0].node);
                    if (name.lexeme == "cos") return std::make_shared<CosNode>(args[0].node);
                    if (name.lexeme == "log") return std::make_shared<LogNode>(args[0].node);
                    if (name.lexeme == "exp") return std::make_shared<ExpNode>(args[0].node);
                    if (name.lexeme == "tan") return std::make_shared<TanNode>(args[0].node);
                }

                // 2. Check Custom Functions (Intelligent Registry)
                auto custom_func = system_.function_registry().get(name.lexeme);
                if (custom_func) {
                    return std::make_shared<CustomFunctionNode>(custom_func, args);
                }
                
                throw error(name, "Unknown function or incorrect arguments: " + name.lexeme);
            }

            // Otherwise, it's a variable or a constant
            auto constant = system_.constant_registry().get(name.lexeme);
            int idx = system_.registry().register_variable(name.lexeme);
            
            // If it's a known constant and hasn't been configured yet, fix it!
            if (constant && !system_.registry().get_variable(idx).is_fixed) {
                system_.registry().set_value(idx, constant->value);
                system_.registry().set_fixed(idx, true);
                if (!constant->unit.empty()) {
                    system_.registry().set_unit(idx, constant->unit);
                }
            }

            return std::make_shared<VariableNode>(idx, name.lexeme);
        }

        if (match(TokenType::LPAREN)) {
            NodePtr node = expression();
            consume(TokenType::RPAREN, "Expect ')' after expression.");
            return node;
        }

        throw error(peek(), "Expect expression.");
    }

    // --- Helpers ---

    double evaluate_const_expression() {
        // Simple version: only parses literal numbers for metadata values
        Token t = consume(TokenType::NUMBER, "Expect numeric value for metadata/exponent.");
        return std::stod(t.lexeme);
    }

    bool match(TokenType t) {
        if (check(t)) { advance(); return true; }
        return false;
    }

    template<typename... Args>
    bool match(Args... types) {
        if ((check(types) || ...)) { advance(); return true; }
        return false;
    }

    bool check(TokenType type) const {
        if (is_at_end()) return false;
        return peek().type == type;
    }

    Token advance() {
        if (!is_at_end()) current_++;
        return previous();
    }

    bool is_at_end() const { return peek().type == TokenType::END_OF_FILE; }
    Token peek() const { return tokens_[current_]; }
    Token peek_next() const { 
        if (is_at_end()) return tokens_[current_];
        return tokens_[current_ + 1]; 
    }
    Token previous() const { return tokens_[current_ - 1]; }

    Token consume(TokenType type, const std::string& message) {
        if (check(type)) return advance();
        throw error(peek(), message);
    }

    std::runtime_error error(Token token, const std::string& message) {
        return std::runtime_error("[Line " + std::to_string(token.line) + "] Error at '" + token.lexeme + "': " + message);
    }
};

} // namespace cones

#endif
