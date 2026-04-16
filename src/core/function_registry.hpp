#ifndef CONES_CORE_FUNCTION_REGISTRY_HPP
#define CONES_CORE_FUNCTION_REGISTRY_HPP

#include "dual_number.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <stdexcept>

namespace cones {

/**
 * @brief Represents an argument passed to a function, optionally named.
 */
struct FuncArg {
    std::string name; // Empty if positional
    DualNumber value;
};

/**
 * @brief Interface for "Intelligent" functions (like Property lookups).
 */
class IFunction {
public:
    virtual ~IFunction() = default;
    virtual std::string name() const = 0;

    /**
     * @brief Validates inputs (e.g., "Do I have 2 properties for the state?")
     */
    virtual void validate(const std::vector<FuncArg>& args) const = 0;

    /**
     * @brief Computes the result using Dual Numbers.
     */
    virtual DualNumber evaluate(const std::vector<FuncArg>& args) const = 0;
};

/**
 * @brief Global registry for functions available in .cnes scripts.
 */
class FunctionRegistry {
    std::map<std::string, std::shared_ptr<IFunction>> functions_;

public:
    void register_function(std::shared_ptr<IFunction> func) {
        functions_[func->name()] = func;
    }

    std::shared_ptr<IFunction> get(const std::string& name) const {
        auto it = functions_.find(name);
        if (it == functions_.end()) return nullptr;
        return it->second;
    }
};

} // namespace cones

#endif
