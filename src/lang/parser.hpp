#ifndef CONES_LANG_PARSER_HPP
#define CONES_LANG_PARSER_HPP

#include "lexer.hpp"
#include "../core/system.hpp"
#include <vector>
#include <stdexcept>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <map>

namespace cones {

class Parser {
    std::vector<Token> tokens_;
    int current_ = 0;
    System& system_;
    bool is_local_parsing_ = false;
    std::vector<std::string> local_param_names_;
    std::vector<std::filesystem::path> search_paths_;
    std::filesystem::path exe_path_;
    int include_depth_ = 0;
    static constexpr int MAX_INCLUDE_DEPTH = 10;

    public:
    Parser(std::vector<Token> tokens, System& sys, std::filesystem::path initial_path = ".", int depth = 0) 
        : tokens_(std::move(tokens)), system_(sys), include_depth_(depth) {
        if (include_depth_ > MAX_INCLUDE_DEPTH) {
            throw std::runtime_error("Maximum include depth exceeded (circular dependency?)");
        }
        std::filesystem::path abs_path = std::filesystem::absolute(initial_path);
        if (std::filesystem::is_directory(abs_path)) {
            search_paths_.push_back(abs_path);
        } else {
            search_paths_.push_back(abs_path.parent_path());
        }
    }

    void set_exe_path(std::filesystem::path p) { exe_path_ = std::move(p); }

    void parse() {
        try {
            scrap_definitions();
            while (!is_at_end()) statement();
        } catch (const std::bad_alloc&) {
            throw std::runtime_error("Parser: Out of memory. Possible infinite expansion in routines or includes.");
        } catch (const std::exception& e) {
            throw std::runtime_error("Parser: " + std::string(e.what()));
        }
    }

private:
    void scrap_definitions() {
        current_ = 0;
        std::vector<Token> filtered;

        while (!is_at_end()) {
            if (match(TokenType::ROUTINE)) {
                RoutineDef def;
                def.name = consume(TokenType::IDENTIFIER, "Expect routine name.").lexeme;
                consume(TokenType::LPAREN, "Expect ( after routine name.");
                if (!check(TokenType::RPAREN)) {
                    do {
                        def.params.push_back(consume(TokenType::IDENTIFIER, "Expect parameter name.").lexeme);
                    } while (match(TokenType::COMMA));
                }
                consume(TokenType::RPAREN, "Expect ) after parameters.");

                while (!(check(TokenType::END) && peek_next().type == TokenType::ROUTINE)) {
                    if (is_at_end()) throw error(peek(), "Unterminated routine.");
                    def.body_tokens.push_back(advance());
                }
                consume(TokenType::END, "Expect end.");
                consume(TokenType::ROUTINE, "Expect routine.");
                system_.definition_registry().register_routine(std::move(def));
            } else if (match(TokenType::FUNCTION)) {
                FunctionDef def;
                def.name = consume(TokenType::IDENTIFIER, "Expect function name.").lexeme;
                consume(TokenType::LPAREN, "Expect ( after function name.");
                if (!check(TokenType::RPAREN)) {
                    do {
                        def.params.push_back(consume(TokenType::IDENTIFIER, "Expect parameter name.").lexeme);
                    } while (match(TokenType::COMMA));
                }
                consume(TokenType::RPAREN, "Expect ) after parameters.");

                is_local_parsing_ = true;
                while (!check(TokenType::END) && !is_at_end()) {
                    if (match(TokenType::RETURN)) {
                        def.return_node = expression();
                        if (match(TokenType::LBRACKET)) {
                            std::string uname = "";
                            while(!check(TokenType::RBRACKET) && !is_at_end()) uname += advance().lexeme;
                            consume(TokenType::RBRACKET, "Expect ].");
                            def.return_unit = Unit::from_string(uname);
                        } else def.return_unit = Unit::Dimensionless();
                    } else {
                        std::string lhs = consume(TokenType::IDENTIFIER, "Expect variable name in function.").lexeme;
                        consume(TokenType::EQUALS, "Expect =.");
                        def.body_assignments.push_back({lhs, expression()});
                    }
                }
                is_local_parsing_ = false;
                consume(TokenType::END, "Expect end function.");
                consume(TokenType::FUNCTION, "Expect 'function'.");
                system_.definition_registry().register_function(std::move(def));
            } else {
                filtered.push_back(advance());
            }
        }

        // CRITICAL: Preserve EOF token
        if (current_ < (int)tokens_.size() && tokens_[current_].type == TokenType::END_OF_FILE) {
            filtered.push_back(tokens_[current_]);
        } else {
            filtered.emplace_back(TokenType::END_OF_FILE, "", 0);
        }

        tokens_ = std::move(filtered);
        current_ = 0;
    }

    std::filesystem::path resolve_path(const std::string& original_path) {
        std::filesystem::path p(original_path);
        std::vector<std::string> trials;
        
        trials.push_back(original_path);
        if (p.extension().empty()) {
            trials.push_back(original_path + ".cnes");
        }

        for (const auto& trial_str : trials) {
            std::filesystem::path trial(trial_str);

            if (!search_paths_.empty()) {
                std::filesystem::path rel = search_paths_.back() / trial;
                if (std::filesystem::exists(rel)) return rel;
            }

            if (std::filesystem::exists(trial)) return trial;

            if (!exe_path_.empty()) {
                std::filesystem::path lib_path = exe_path_ / "libs" / trial;
                if (std::filesystem::exists(lib_path)) return lib_path;
            }
        }

        return "";
    }

    void include_statement() {
        Token path_token = consume(TokenType::STRING, "Expect file path after include.");
        std::string raw_path = path_token.lexeme;

        std::filesystem::path resolved = resolve_path(raw_path);
        if (resolved.empty()) {
            throw error(path_token, "Could not resolve include path: " + raw_path);
        }

        std::ifstream file(resolved);
        if (!file.is_open()) {
            throw error(path_token, "Could not open included file: " + resolved.string());
        }

        std::stringstream ss;
        ss << file.rdbuf();

        Lexer lex(ss.str());
        std::vector<Token> included_tokens = lex.scan_tokens();

        // Create a sub-parser with incremented depth
        Parser sub_parser(included_tokens, system_, resolved, include_depth_ + 1);
        sub_parser.set_exe_path(exe_path_);
        sub_parser.parse();
    }


    void statement() {
        if (match(TokenType::INCLUDE)) {
            include_statement();
            return;
        }
        if (check(TokenType::IDENTIFIER)) {
            Token name = peek();
            TokenType next_t = peek_next().type;
            
            if (next_t == TokenType::LPAREN && system_.definition_registry().get_routine(name.lexeme)) {
                expand_routine();
                return;
            }

            if (next_t == TokenType::COLON_EQUALS || next_t == TokenType::DOT || next_t == TokenType::LBRACKET || next_t == TokenType::LBRACE) {
                definition_statement();
                return;
            }
        }
        equation_statement();
    }

    void expand_routine() {
        Token name = advance();
        const RoutineDef* def = system_.definition_registry().get_routine(name.lexeme);
        
        consume(TokenType::LPAREN, "Expect (.");
        std::vector<Token> args;
        if (!check(TokenType::RPAREN)) {
            do {
                args.push_back(advance());
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RPAREN, "Expect ).");

        if (args.size() != def->params.size()) {
            throw error(name, "Routine " + name.lexeme + " requires " + std::to_string(def->params.size()) + " arguments.");
        }

        std::map<std::string, Token> mapping;
        for (size_t i = 0; i < args.size(); ++i) {
            mapping.emplace(def->params[i], args[i]);
        }

        std::vector<Token> expanded;
        for (const auto& body_token : def->body_tokens) {
            auto it = mapping.find(body_token.lexeme);
            if (it != mapping.end() && body_token.type == TokenType::IDENTIFIER) {
                expanded.push_back(it->second);
            } else {
                expanded.push_back(body_token);
            }
        }

        if (tokens_.size() + expanded.size() > 1000000) {
            throw std::runtime_error("Parser: Script too large after routine expansion (possible infinite recursion).");
        }

        tokens_.insert(tokens_.begin() + current_, expanded.begin(), expanded.end());
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

                auto user_func = system_.definition_registry().get_function(name.lexeme);
                if (user_func) {
                    std::vector<NodePtr> arg_nodes;
                    for (const auto& arg : args) arg_nodes.push_back(arg.node);
                    return std::make_shared<UserFunctionNode>(user_func, arg_nodes);
                }

                throw error(name, "Unknown function.");
            }
            
            if (is_local_parsing_) {
                return std::make_shared<LocalVariableNode>(name.lexeme);
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
    bool is_at_end() const { return current_ >= (int)tokens_.size() || tokens_[current_].type == TokenType::END_OF_FILE; }
    Token peek() const { return tokens_[current_]; }
    Token peek_next() const { 
        if (current_ + 1 >= (int)tokens_.size()) return tokens_.back();
        return tokens_[current_ + 1]; 
    }
    Token previous() const { 
        if (current_ == 0) return tokens_[0];
        return tokens_[current_ - 1]; 
    }
    Token consume(TokenType type, const std::string& message) { if (check(type)) return advance(); throw error(peek(), message); }
    std::runtime_error error(Token token, const std::string& message) { return std::runtime_error("[Line " + std::to_string(token.line) + "] Error at " + token.lexeme + ": " + message); }
};
} // namespace cones
#endif
