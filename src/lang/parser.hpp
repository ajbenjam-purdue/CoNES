#ifndef CONES_LANG_PARSER_HPP
#define CONES_LANG_PARSER_HPP

#include "lexer.hpp"
#include "../core/system.hpp"
#include "../core/unit_parser.hpp"
#include <vector>
#include <stdexcept>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <map>
#include <iostream>

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
    // Create a parser instance with a set of tokens and a defined system
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

    // Overwrite the executable for the parser
    void set_exe_path(std::filesystem::path p) { exe_path_ = std::move(p); }

    // Attempt to parse the held tokens
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

    // Consume and add to the context any functions (contained local) or routines (uncontained local)
    void scrap_definitions() {
        current_ = 0;
        std::vector<Token> filtered;

        while (!is_at_end()) {
            // Routines: create a routine and add to system
            if (match(TokenType::ROUTINE)) {

                // Creation
                RoutineDef def;
                def.name = consume(TokenType::IDENTIFIER, "Expect routine name.").lexeme;

                // Parameters
                consume(TokenType::LPAREN, "Expect ( after routine name.");
                if (!check(TokenType::RPAREN)) {
                    do {
                        def.params.push_back(consume(TokenType::IDENTIFIER, "Expect parameter name.").lexeme);
                    } while (match(TokenType::COMMA));
                }
                consume(TokenType::RPAREN, "Expect ) after parameters.");

                // Body
                while (!(check(TokenType::END) && peek_next().type == TokenType::ROUTINE)) {
                    if (is_at_end()) throw error(peek(), "Unterminated routine."); // TODO: More robust checking
                    def.body_tokens.push_back(advance());
                }
                consume(TokenType::END, "Expect end.");
                consume(TokenType::ROUTINE, "Expect routine.");

                // Register the contents
                system_.definition_registry().register_routine(std::move(def));
            } 
            
            // Functions: create a function and add to system
            else if (match(TokenType::FUNCTION)) {

                // Creation
                FunctionDef def;
                def.name = consume(TokenType::IDENTIFIER, "Expect function name.").lexeme;

                // Parameters
                consume(TokenType::LPAREN, "Expect ( after function name.");
                if (!check(TokenType::RPAREN)) {
                    do {
                        def.params.push_back(consume(TokenType::IDENTIFIER, "Expect parameter name.").lexeme);
                    } while (match(TokenType::COMMA));
                }
                consume(TokenType::RPAREN, "Expect ) after parameters.");

                // PRIVATE local parsing
                is_local_parsing_ = true;
                while (!check(TokenType::END) && !is_at_end()) {

                    // Explicit return value association
                    if (match(TokenType::RETURN)) {
                        def.return_node = expression();
                        if (match(TokenType::LBRACKET)) {
                            Token bracket = previous();
                            std::string uname = "";
                            while(!check(TokenType::RBRACKET) && !is_at_end()) uname += advance().lexeme;
                            consume(TokenType::RBRACKET, "Expect ].");
                            try {
                                def.return_unit = parse_unit(uname);
                                def.return_unit_name = uname;
                            } catch (const std::exception& e) {
                                throw error(bracket, e.what());
                            }
                        } else {
                            def.return_unit = def.return_node->get_unit(system_.registry());
                            if (auto cast = std::dynamic_pointer_cast<UnitCastNode>(def.return_node)) {
                                def.return_unit_name = cast->get_unit_name();
                            } else {
                                def.return_unit_name = def.return_unit.to_string();
                            }
                        }
                    }
                    
                    // Contents
                    else {
                        std::string lhs = consume(TokenType::IDENTIFIER, "Expect variable name in function.").lexeme;
                        consume(TokenType::EQUALS, "Expect =.");
                        def.body_assignments.push_back({lhs, expression()});
                    }
                }

                // End of function
                is_local_parsing_ = false;
                consume(TokenType::END, "Expect end function.");
                consume(TokenType::FUNCTION, "Expect 'function'.");
                system_.definition_registry().register_function(std::move(def));
            } 
            
            // Other: add to filtered list
            else {
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

    // Wrapper to allow dynamic pathing: libs/test.cnes, test.cnes, and test are all the same
    std::filesystem::path resolve_path(const std::string& original_path) {
        std::filesystem::path p(original_path);
        std::vector<std::string> trials;
        
        trials.push_back(original_path);
        if (p.extension().empty()) {
            trials.push_back(original_path + ".cnes");
        }

        // Check each for validity
        for (const auto& trial_str : trials) {
            std::filesystem::path trial(trial_str);

            if (!search_paths_.empty()) { // First, check parent dir
                std::filesystem::path rel = search_paths_.back() / trial;
                if (std::filesystem::exists(rel)) return rel;
            }

            if (std::filesystem::exists(trial)) return trial; // Check the actual path

            if (!exe_path_.empty()) { // Check on the executable/lib path LAST
                std::filesystem::path lib_path = exe_path_ / "libs" / trial;
                if (std::filesystem::exists(lib_path)) return lib_path;
            }
        }

        return "";
    }

    // "Include" and parse inclusion contents
    void include_statement() {
        Token path_token = consume(TokenType::STRING, "Expect file path after include.");
        std::string raw_path = path_token.lexeme;

        // Get and check resolved path
        std::filesystem::path resolved = resolve_path(raw_path);
        if (resolved.empty()) { // Empty if doesn't exist
            throw error(path_token, "Could not resolve include path: " + raw_path);
        }

        // Open file and read
        std::ifstream file(resolved);
        if (!file.is_open()) {
            throw error(path_token, "Could not open included file: " + resolved.string());
        }
        std::stringstream ss;
        ss << file.rdbuf();

        // Tokenize the inclusion
        Lexer lex(ss.str());
        std::vector<Token> included_tokens = lex.scan_tokens();

        // Create a sub-parser with incremented depth
        Parser sub_parser(included_tokens, system_, resolved, include_depth_ + 1);
        sub_parser.set_exe_path(exe_path_);
        sub_parser.parse();
    }

    // Global statement expansion
    void statement() {
        if (match(TokenType::INCLUDE)) { // Include statement
            include_statement();
            return;
        }
        if (check(TokenType::IDENTIFIER)) { // We have an identifier, what we do depends on the following token
            Token name = peek();
            TokenType next_t = peek_next().type;

            // Routine
            if (next_t == TokenType::LPAREN && system_.definition_registry().get_routine(name.lexeme)) {
                expand_routine();
                return;
            }

            // Definition (assignment, equivalency, unit casting)
            if (next_t == TokenType::COLON_EQUALS || next_t == TokenType::DOT || next_t == TokenType::LBRACKET || next_t == TokenType::LBRACE) {
                definition_statement();
                return;
            }
        }
        equation_statement();
    }

    // Expand a routine into global scope, replacing the input tokens with the provided tokens
    void expand_routine() {

        // Get name and definition of the matching routine
        Token name = advance();
        const RoutineDef* def = system_.definition_registry().get_routine(name.lexeme);
        consume(TokenType::LPAREN, "Expect (.");

        // Get parameters
        std::vector<Token> args;
        if (!check(TokenType::RPAREN)) {
            do {
                args.push_back(advance());
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RPAREN, "Expect ).");

        // Not enough/too many parameters
        if (args.size() != def->params.size()) {
            throw error(name, "Routine " + name.lexeme + " requires " + std::to_string(def->params.size()) + " arguments.");
        }

        // Mapping (in-out)
        std::map<std::string, Token> mapping;
        for (size_t i = 0; i < args.size(); ++i) {
            mapping.emplace(def->params[i], args[i]);
        }

        // Expansion
        std::vector<Token> expanded;
        for (const auto& body_token : def->body_tokens) {
            auto it = mapping.find(body_token.lexeme);
            if (it != mapping.end() && body_token.type == TokenType::IDENTIFIER) {
                expanded.push_back(it->second);
            } else {
                expanded.push_back(body_token);
            }
        }

        // TODO: improve recursion detection so this stupid check isn't needed
        if (tokens_.size() + expanded.size() > 1000000) {
            throw std::runtime_error("Parser: Script too large after routine (name: "+def->name+") expansion (possible infinite recursion).");
        }

        // Add into global scope
        tokens_.insert(tokens_.begin() + current_, expanded.begin(), expanded.end());
    }

    Unit parse_unit(const std::string& name) {
        if (name.empty()) return Unit::Dimensionless();
        UnitParser unit_parser(system_.unit_registry());
        return unit_parser.parse(name);
    }
    // Associate something with something else
    void definition_statement() {
        Token name = consume(TokenType::IDENTIFIER, "Expect name.");
        auto& reg = system_.registry();
        int idx = reg.register_variable(name.lexeme, name.line);

        // attribute (.unit, .lower, .upper, or .guess)
        if (match(TokenType::DOT)) {
            Token attr = consume(TokenType::IDENTIFIER, "Expect attribute.");
            consume(TokenType::COLON_EQUALS, "Expect :=.");
            if (attr.lexeme == "unit") { // Unit
                std::string unit_val = "";
                Token start_token = peek();
                if (match(TokenType::LBRACKET)) {
                    start_token = previous();
                    while (!check(TokenType::RBRACKET) && !is_at_end()) unit_val += advance().lexeme;
                    consume(TokenType::RBRACKET, "Expect ].");
                } else unit_val = consume(TokenType::IDENTIFIER, "Expect unit.").lexeme;
                
                try {
                    Unit u = parse_unit(unit_val);
                    reg.set_unit(idx, u, unit_val);
                    reg.suggest_guess(idx, u);
                } catch (const std::exception& e) {
                    throw error(start_token, e.what());
                }
            } else { // Guess, Lower, or Upper bound
                NodePtr rhs = expression();
                double val = evaluate_to_double(rhs);
                bool is_si = !rhs->get_unit(reg).is_dimensionless();

                if (attr.lexeme == "guess") reg.set_value(idx, val, is_si);
                else if (attr.lexeme == "lower") reg.set_lower_bound(idx, val, is_si);
                else if (attr.lexeme == "upper") reg.set_upper_bound(idx, val, is_si);
                else throw error(name, "Unknown property ."+attr.lexeme+" (must be .unit, .lower, .upper, or .guess).");
            }
        } 
        
        // Definition (y:=x) or guess/lower/upper (y[x:x:x])
        else if (match(TokenType::COLON_EQUALS) || check(TokenType::LBRACE)) {
            bool has_assign = (previous().type == TokenType::COLON_EQUALS);
            if (match(TokenType::LBRACE)) {
                if (!match(TokenType::UNDERSCORE)) reg.set_value(idx, evaluate_to_double(expression()), true);
                consume(TokenType::COLON, "Expect :.");
                if (!match(TokenType::UNDERSCORE)) reg.set_lower_bound(idx, evaluate_to_double(expression()), true);
                consume(TokenType::COLON, "Expect :.");
                if (!match(TokenType::UNDERSCORE)) reg.set_upper_bound(idx, evaluate_to_double(expression()), true);
                consume(TokenType::RBRACE, "Expect }.");
            }
            else if (has_assign) {
                NodePtr rhs = expression();
                if (reg.get_variable(idx).unit.is_dimensionless()) {
                    Unit u = rhs->get_unit(reg);
                    std::string u_name = u.to_string();
                    if (auto cast = std::dynamic_pointer_cast<UnitCastNode>(rhs)) {
                        u_name = cast->get_unit_name();
                    } else if (auto func = std::dynamic_pointer_cast<UserFunctionNode>(rhs)) {
                        u_name = func->get_unit_name();
                    }
                    if (!u.is_dimensionless()) reg.set_unit(idx, u, u_name);
                }
                DualNumber res = evaluate_dual(rhs); // static, NO dynamic assignments
                reg.set_value(idx, res.val, true);
                reg.set_fixed(idx, true);
            }
        }
    }

    // Relate something to something else
    void equation_statement() {
        int line = peek().line;

        // Get expression
        NodePtr lhs = expression();
        consume(TokenType::EQUALS, "Expect =.");
        NodePtr rhs = expression();

        // If the variable can successfuly be ID'd, parse
        if (auto var_node = std::dynamic_pointer_cast<VariableNode>(lhs)) {
            auto& var_info = system_.registry().get_variable(system_.registry().get_index(var_node->to_string()));
            if (var_info.unit.is_dimensionless()) { // In the event there is no assigned unit, try to assume one AND guess an initial val
                Unit inherited = rhs->get_unit(system_.registry());
                std::string u_name = inherited.to_string();
                if (auto cast = std::dynamic_pointer_cast<UnitCastNode>(rhs)) {
                    u_name = cast->get_unit_name();
                } else if (auto func = std::dynamic_pointer_cast<UserFunctionNode>(rhs)) {
                    u_name = func->get_unit_name();
                }
                system_.registry().set_unit(var_info.index, inherited, u_name);
                system_.registry().suggest_guess(var_info.index, inherited);
            }
        }

        // Add eqn to the registry
        system_.add_equation(std::make_shared<SubNode>(lhs, rhs), line);
    }

    // Binary Tree-esque structure
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
        if (match(TokenType::CARET)) node = std::make_shared<PowNode>(node, evaluate_to_double(unary()));
        return node;
    }
    NodePtr primary() {
        NodePtr node = primary_base();
        if (match(TokenType::LBRACKET)) {
            Token bracket = previous();
            std::string unit_name = "";
            while (!check(TokenType::RBRACKET) && !is_at_end()) unit_name += advance().lexeme;
            consume(TokenType::RBRACKET, "Expect ].");
            try {
                node = std::make_shared<UnitCastNode>(node, node->get_unit(system_.registry()), parse_unit(unit_name), unit_name);
            } catch (const std::exception& e) {
                throw error(bracket, e.what());
            }
        }
        return node;
    }

    NodePtr primary_base() {
        if (match(TokenType::NUMBER)) {
            auto node = std::make_shared<ConstantNode>(std::stod(previous().lexeme));
            node->set_line(previous().line);
            return node;
        }
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
                if (custom_func) {
                    for (auto &arg : args) {
                        if (auto var_node = std::dynamic_pointer_cast<VariableNode>(arg.node)) {
                            int idx = var_node->get_index();
                            auto &var_info = system_.registry().get_variable(idx);
                            if (var_info.unit.is_dimensionless() && !arg.name.empty()) {
                                Unit inferred = Unit::Dimensionless();
                                std::string u_name = "";
                                if (arg.name == "T" || arg.name == "Temperature") { inferred = Unit::Kelvin(); u_name = "K"; }
                                else if (arg.name == "P" || arg.name == "Pressure") { inferred = Unit::Pascal(); u_name = "Pa"; }
                                else if (arg.name == "h" || arg.name == "Enthalpy") { inferred = Unit(1.0, {0, 2, -2, 0, 0, 0}); u_name = "J/kg"; }
                                else if (arg.name == "s" || arg.name == "Entropy") { inferred = Unit(1.0, {0, 2, -2, -1, 0, 0}); u_name = "J/(kg*K)"; }
                                else if (arg.name == "u" || arg.name == "InternalEnergy") { inferred = Unit(1.0, {0, 2, -2, 0, 0, 0}); u_name = "J/kg"; }
                                else if (arg.name == "rho" || arg.name == "Density") { inferred = Unit(1.0, {1, -3, 0, 0, 0, 0}); u_name = "kg/m^3"; }
                                else if (arg.name == "v" || arg.name == "SpecificVolume") { inferred = Unit(1.0, {-1, 3, 0, 0, 0, 0}); u_name = "m^3/kg"; }
                                else if (arg.name == "mu" || arg.name == "Viscosity") { inferred = Unit(1.0, {1, -1, -1, 0, 0, 0}); u_name = "Pa*s"; }
                                else if (arg.name == "k" || arg.name == "Conductivity") { inferred = Unit(1.0, {1, 1, -3, -1, 0, 0}); u_name = "W/m*K"; }

                                if (!inferred.is_dimensionless()) {
                                    system_.registry().set_unit(idx, inferred, u_name);
                                    system_.registry().suggest_guess(idx, inferred);
                                }
                            }
                        }
                    }
                    auto node = std::make_shared<CustomFunctionNode>(custom_func, args);
                    node->set_line(name.line);
                    return node;
                }

                auto user_func = system_.definition_registry().get_function(name.lexeme);
                if (user_func) {
                    std::vector<NodePtr> arg_nodes;
                    for (const auto& arg : args) arg_nodes.push_back(arg.node);
                    auto node = std::make_shared<UserFunctionNode>(user_func, arg_nodes);
                    node->set_line(name.line);
                    return node;
                }

                throw error(name, "Unknown function.");
            }
            
            if (is_local_parsing_) {
                auto node = std::make_shared<LocalVariableNode>(name.lexeme);
                node->set_line(name.line);
                return node;
            }

            auto constant = system_.constant_registry().get(name.lexeme);
            auto substance = system_.substance_manager().get(name.lexeme);
            int idx = system_.registry().register_variable(name.lexeme, name.line);
            if (constant && !system_.registry().get_variable(idx).is_fixed) {
                system_.registry().set_value(idx, constant->value, true);
                system_.registry().set_fixed(idx, true);
                system_.registry().set_reserved(idx, true);
                if (!constant->unit.empty()) {
                    Unit u = parse_unit(constant->unit);
                    system_.registry().set_unit(idx, u, constant->unit);
                }
            }
            if (substance) { system_.registry().set_fixed(idx, true); system_.registry().set_reserved(idx, true); }
            auto node = std::make_shared<VariableNode>(idx, name.lexeme);
            node->set_line(name.line);
            return node;
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
