#ifndef CONES_CORE_FUNCTION_REGISTRY_HPP
#define CONES_CORE_FUNCTION_REGISTRY_HPP

#include "dual_number.hpp"
#include "unit.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <stdexcept>

namespace cones {

struct FuncArg {
    std::string name;
    DualNumber value;
    Unit unit;
};

class IFunction {
public:
    virtual ~IFunction() = default;
    virtual std::string name() const = 0;
    virtual std::string args_metadata() const = 0;
    virtual std::string description() const = 0;
    virtual void validate(const std::vector<FuncArg>& args) const = 0;
    virtual DualNumber evaluate(const std::vector<FuncArg>& args) const = 0;
    virtual Unit get_unit(const std::vector<Unit>& input_units) const = 0;
};

class FunctionRegistry {
    std::map<std::string, std::shared_ptr<IFunction>> functions_;
public:
    void register_function(std::shared_ptr<IFunction> func) { functions_[func->name()] = func; }
    std::shared_ptr<IFunction> get(const std::string& name) const {
        auto it = functions_.find(name);
        return (it == functions_.end()) ? nullptr : it->second;
    }
    std::vector<std::string> get_function_names() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : functions_) names.push_back(name);
        return names;
    }
    std::vector<std::string> get_function_metadata() const {
        std::vector<std::string> metadata;
        for (const auto& [name, func] : functions_) {
            metadata.push_back(name + ":" + func->args_metadata() + ":" + func->description());
        }
        return metadata;
    }
};

} // namespace cones

#endif
