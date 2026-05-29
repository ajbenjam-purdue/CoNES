#ifndef CONES_LANG_DEFINITION_REGISTRY_HPP
#define CONES_LANG_DEFINITION_REGISTRY_HPP

#include "token.hpp"
#include "../core/unit.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace cones {

// Forward declaration
class Node;
using NodePtr = std::shared_ptr<Node>;

struct RoutineDef {
    std::string name;
    std::vector<std::string> params;
    std::vector<Token> body_tokens;
};

struct FunctionAssign {
    std::string lhs_name;
    NodePtr rhs_node;
};

struct FunctionDef {
    std::string name;
    std::vector<std::string> params;
    std::vector<FunctionAssign> body_assignments;
    NodePtr return_node;
    Unit return_unit;
    std::string return_unit_name;
};

class DefinitionRegistry {
    std::map<std::string, RoutineDef> routines_;
    std::map<std::string, FunctionDef> functions_;

public:
    void register_routine(RoutineDef def) { routines_[def.name] = std::move(def); }
    void register_function(FunctionDef def) { functions_[def.name] = std::move(def); }

    const RoutineDef* get_routine(const std::string& name) const {
        auto it = routines_.find(name);
        return (it != routines_.end()) ? &it->second : nullptr;
    }

    const FunctionDef* get_function(const std::string& name) const {
        auto it = functions_.find(name);
        return (it != functions_.end()) ? &it->second : nullptr;
    }

    std::vector<std::string> get_routine_names() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : routines_) names.push_back(name);
        return names;
    }

    std::vector<std::string> get_function_names() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : functions_) names.push_back(name);
        return names;
    }
};

} // namespace cones

#endif
