#ifndef CONES_CORE_UNIT_PARSER_HPP
#define CONES_CORE_UNIT_PARSER_HPP

#include "unit.hpp"
#include "unit_registry.hpp"
#include <string>
#include <vector>
#include <stdexcept>
#include <cctype>

namespace cones {

/**
 * @brief A simple recursive-descent parser for compound unit strings.
 * Supports: *, /, ^ and parenthesis.
 * Example: "W / (m^2 * K)"
 */
class UnitParser {
    const UnitRegistry& registry_;
    std::string source_;
    size_t pos_ = 0;

public:
    explicit UnitParser(const UnitRegistry& registry) : registry_(registry) {}

    Unit parse(const std::string& expression) {
        source_ = expression;
        pos_ = 0;
        Unit result = parse_expression();
        skip_whitespace();
        if (pos_ < source_.length()) {
            throw std::runtime_error("Unexpected character in unit: '" + std::string(1, source_[pos_]) + "'");
        }
        return result;
    }

private:
    Unit parse_expression() {
        Unit result = parse_term();
        while (true) {
            skip_whitespace();
            if (match('*')) {
                result = result * parse_term();
            } else if (match('/')) {
                result = result / parse_term();
            } else {
                break;
            }
        }
        return result;
    }

    Unit parse_term() {
        Unit result = parse_factor();
        skip_whitespace();
        if (match('^')) {
            double power = parse_number();
            result = result.pow(power);
        }
        return result;
    }

    Unit parse_factor() {
        skip_whitespace();
        if (match('(')) {
            Unit result = parse_expression();
            if (!match(')')) {
                throw std::runtime_error("Expected ')' in unit expression");
            }
            return result;
        }

        std::string name = parse_identifier();
        if (name.empty()) {
            // Check if it's just a number (e.g. 1/s)
            try {
                double val = parse_number();
                return Unit::Dimensionless() * val;
            } catch (...) {
                throw std::runtime_error("Expected unit name or '('");
            }
        }

        const UnitDefinition* def = registry_.get(name);
        if (!def) {
            throw std::runtime_error("Unknown unit: \"" + name + "\"");
        }
        return def->unit;
    }

    std::string parse_identifier() {
        std::string result;
        while (pos_ < source_.length() && (std::isalnum(source_[pos_]) || source_[pos_] == '_')) {
            result += source_[pos_++];
        }
        return result;
    }

    double parse_number() {
        skip_whitespace();
        size_t start = pos_;
        if (pos_ < source_.length() && (source_[pos_] == '-' || source_[pos_] == '+')) {
            pos_++;
        }
        while (pos_ < source_.length() && (std::isdigit(source_[pos_]) || source_[pos_] == '.')) {
            pos_++;
        }
        if (start == pos_) throw std::runtime_error("Expected number");
        return std::stod(source_.substr(start, pos_ - start));
    }

    void skip_whitespace() {
        while (pos_ < source_.length() && std::isspace(source_[pos_])) {
            pos_++;
        }
    }

    bool match(char expected) {
        if (pos_ < source_.length() && source_[pos_] == expected) {
            pos_++;
            return true;
        }
        return false;
    }
};

} // namespace cones

#endif
