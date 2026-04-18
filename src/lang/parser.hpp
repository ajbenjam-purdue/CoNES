#ifndef CONES_LANG_PARSER_HPP
#define CONES_LANG_PARSER_HPP

#include "lexer.hpp"
#include "../core/system.hpp"
#include <vector>
#include <stdexcept>
#include <string>
#include <fstream>
#include <sstream>

namespace cones {

class Parser {
    std::vector<Token> tokens_;
    int current_ = 0;
    System& system_;

public:
    Parser(std::vector<Token> tokens, System& sys) : tokens_(std::move(tokens)), system_(sys) {}

    void parse() {
        while (!is_at_end()) statement();
    }

private:
    void statement() {
        if (match(TokenType::INCLUDE)) {
            include_statement();
            return;
        }
        if (check(TokenType::IDENTIFIER)) {
            TokenType t = peek_next().type;
            if (t == TokenType::COLON_EQUALS || t == TokenType::DOT || t == TokenType::LBRACKET || t == TokenType::LBRACE) {
                definition_statement();
                return;
            }
        }
        equation_statement();
    }

    void include_statement() {
        Token path_token = consume(TokenType::STRING, "Expect file path after include.");
        std::string path = path_token.lexeme;
        
        std::ifstream file(path);
        if (!file.is_open()) {
            throw error(path_token, "Could not open included file: " + path);
        }

        std::stringstream ss;
        ss << file.rdbuf();
        Lexer lex(ss.str());
        std::vector<Token> included_tokens = lex.scan_tokens();

        // Remove END_OF_FILE from included tokens
        if (!included_tokens.empty() && included_tokens.back().type == TokenType::END_OF_FILE) {
            included_tokens.pop_back();
        }

        // Insert included tokens into the current token stream after the current position
        tokens_.insert(tokens_.begin() + current_, included_tokens.begin(), included_tokens.end());
    }

    void definition_statement() {
        Token name = consume(TokenType::IDENTIFIER, "Expect name.");
        auto& reg = system_.registry();
        int idx = reg.register_variable(name.lexeme);

        if (match(TokenType::DOT)) {
            Token attr = consume(TokenType::IDENTIFIER, "Expect attribute.");
            consume(TokenType::COLON_EQUALS, "Expect :=.");
            if (attr.lexeme == "unit") {
                std::string unit_val = "";
                if (match(TokenType::LBRACKET)) {
                    while (!check(TokenType::RBRACKET) && !is_at_end()) unit_val += advance().lexeme;
                    consume(TokenType::RBRACKET, "Expect ].");
                } else unit_val = consume(TokenType::IDENTIFIER, "Expect unit.").lexeme;
                
                Unit u = Unit::from_string(unit_val);
                if (reg.get_variable(idx).unit.is_dimensionless() && !u.is_dimensionless()) {
                    double v = reg.get_variable(idx).value;
                    reg.set_value(idx, (v + u.offset) * u.scale);
                    double l = reg.get_variable(idx).lower_bound;
                    double up = reg.get_variable(idx).upper_bound;
                    if (l > -1e30) l = (l + u.offset) * u.scale;
                    if (up < 1e30) up = (up + u.offset) * u.scale;
                    reg.set_bounds(idx, l, up);
                }
                reg.set_unit(idx, u, unit_val);
                reg.suggest_guess(idx, u);
            } else {
                double val = evaluate_to_double(expression());
                if (attr.lexeme == "guess") reg.set_value(idx, val);
                else if (attr.lexeme == "lower") reg.set_bounds(idx, val, reg.get_variable(idx).upper_bound);
                else if (attr.lexeme == "upper") reg.set_bounds(idx, reg.get_variable(idx).lower_bound, val);
            }
        } else if (match(TokenType::COLON_EQUALS) || check(TokenType::LBRACE)) {
            bool has_assign = (previous().type == TokenType::COLON_EQUALS);
            if (match(TokenType::LBRACE)) {
                if (!match(TokenType::UNDERSCORE)) reg.set_value(idx, evaluate_to_double(expression()));
                consume(TokenType::COLON, "Expect :.");
                if (!match(TokenType::UNDERSCORE)) reg.set_bounds(idx, evaluate_to_double(expression()), reg.get_variable(idx).upper_bound);
                consume(TokenType::COLON, "Expect :.");
                if (!match(TokenType::UNDERSCORE)) reg.set_bounds(idx, reg.get_variable(idx).lower_bound, evaluate_to_double(expression()));
                consume(TokenType::RBRACE, "Expect }.");
            } else if (has_assign) {
                NodePtr rhs = expression();
                DualNumber res = evaluate_dual(rhs);
                reg.set_value(idx, res.val);
                reg.set_fixed(idx, true);
                if (reg.get_variable(idx).unit.is_dimensionless()) {
                    Unit u = rhs->get_unit(reg);
                    if (!u.is_dimensionless()) reg.set_unit(idx, u, u.to_string());
                }
            }
        }
    }

    void equation_statement() {
        NodePtr lhs = expression();
        consume(TokenType::EQUALS, "Expect =.");
        NodePtr rhs = expression();
        if (auto var_node = std::dynamic_pointer_cast<VariableNode>(lhs)) {
            auto& var_info = system_.registry().get_variable(system_.registry().get_index(var_node->to_string()));
            if (var_info.unit.is_dimensionless()) {
                Unit inherited = rhs->get_unit(system_.registry());
                system_.registry().set_unit(var_info.index, inherited, inherited.to_string());
                system_.registry().suggest_guess(var_info.index, inherited);
            }
        }
        system_.add_equation(std::make_shared<SubNode>(lhs, rhs));
    }

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
        if (match(TokenType::MINUS)) return std::make_shared<NegNode>(unary());
        return power();
    }
    NodePtr power() {
        NodePtr node = primary();
        if (match(TokenType::CARET)) node = std::make_shared<PowNode>(node, evaluate_to_double(expression()));
        return node;
    }
    NodePtr primary() {
        NodePtr node = primary_base();
        if (match(TokenType::LBRACKET)) {
            std::string unit_name = "";
            while (!check(TokenType::RBRACKET) && !is_at_end()) unit_name += advance().lexeme;
            consume(TokenType::RBRACKET, "Expect ].");
            node = std::make_shared<UnitCastNode>(node, node->get_unit(system_.registry()), Unit::from_string(unit_name));
        }
        return node;
    }

    NodePtr primary_base() {
        if (match(TokenType::NUMBER)) return std::make_shared<ConstantNode>(std::stod(previous().lexeme));
        if (match(TokenType::IDENTIFIER)) {
            Token name = previous();
            if (match(TokenType::LPAREN)) {
                std::vector<NodeArg> args;
                if (!check(TokenType::RPAREN)) {
                    do {
                        std::string arg_name = "";
                        if (check(TokenType::IDENTIFIER) && peek_next().type == TokenType::EQUALS) {
                            arg_name = advance().lexeme;
                            consume(TokenType::EQUALS, "Expect =.");
                        }
                        args.push_back({arg_name, expression()});
                    } while (match(TokenType::COMMA));
                }
                consume(TokenType::RPAREN, "Expect ).");
                auto custom_func = system_.function_registry().get(name.lexeme);
                if (custom_func) return std::make_shared<CustomFunctionNode>(custom_func, args);
                throw error(name, "Unknown function.");
            }
            auto constant = system_.constant_registry().get(name.lexeme);
            auto substance = system_.substance_manager().get(name.lexeme);
            int idx = system_.registry().register_variable(name.lexeme);
            if (constant && !system_.registry().get_variable(idx).is_fixed) {
                system_.registry().set_value(idx, constant->value);
                system_.registry().set_fixed(idx, true);
                system_.registry().set_reserved(idx, true);
                if (!constant->unit.empty()) system_.registry().set_unit(idx, constant->unit);
            }
            if (substance) { system_.registry().set_fixed(idx, true); system_.registry().set_reserved(idx, true); }
            return std::make_shared<VariableNode>(idx, name.lexeme);
        }
        if (match(TokenType::LPAREN)) {
            NodePtr node = expression();
            consume(TokenType::RPAREN, "Expect ).");
            return node;
        }
        throw error(peek(), "Expect expression.");
    }

    double evaluate_to_double(NodePtr node) { return evaluate_dual(node).val; }
    DualNumber evaluate_dual(NodePtr node) {
        const auto& reg = system_.registry();
        std::vector<DualNumber> vals;
        vals.reserve(reg.size());
        for (size_t i = 0; i < reg.size(); ++i) vals.push_back({reg.get_variable(i).value, 0.0});
        return node->evaluate(vals, reg);
    }
    bool match(TokenType t) { if (check(t)) { advance(); return true; } return false; }
    template<typename... Args> bool match(Args... types) { if ((check(types) || ...)) { advance(); return true; } return false; }
    bool check(TokenType type) const { return !is_at_end() && peek().type == type; }
    Token advance() { if (!is_at_end()) current_++; return previous(); }
    bool is_at_end() const { return peek().type == TokenType::END_OF_FILE; }
    Token peek() const { return tokens_[current_]; }
    Token peek_next() const { return is_at_end() ? tokens_[current_] : tokens_[current_ + 1]; }
    Token previous() const { return tokens_[current_ - 1]; }
    Token consume(TokenType type, const std::string& message) { if (check(type)) return advance(); throw error(peek(), message); }
    std::runtime_error error(Token token, const std::string& message) { return std::runtime_error("[Line " + std::to_string(token.line) + "] Error at " + token.lexeme + ": " + message); }
};
} // namespace cones
#endif
