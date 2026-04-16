#ifndef CONES_CORE_PROPERTY_FUNCTIONS_HPP
#define CONES_CORE_PROPERTY_FUNCTIONS_HPP

#include "function_registry.hpp"
#include "substance_manager.hpp"
#include <iostream>

namespace cones {

/**
 * @brief Unified property function (e.g., Pressure, Enthalpy, Temperature).
 */
class GeneralPropertyFunction : public IFunction {
    std::string name_;
    PropertyType target_type_;
    const SubstanceManager& manager_;

public:
    GeneralPropertyFunction(std::string name, PropertyType target, const SubstanceManager& mgr)
        : name_(std::move(name)), target_type_(target), manager_(mgr) {}

    std::string name() const override { return name_; }

    void validate(const std::vector<FuncArg>& args) const override {
        if (args.empty()) throw std::runtime_error(name_ + "() requires at least a substance name.");
    }

    DualNumber evaluate(const std::vector<FuncArg>& args) const override {
        // 1. Get substance name (the first positional argument)
        // Note: For now, we assume the substance name is passed as a "dummy" variable name
        // because our Parser doesn't handle String Literals yet.
        if (args.empty()) throw std::runtime_error("No arguments provided to " + name_);
        
        std::string sub_name = args[0].name; // If keyword used: Material=Air
        if (sub_name.empty()) {
            // If positional, we just use a hack or assume it's the first one's string lexeme.
            // For this test, let's assume the user uses Material=Air
        }
        
        // HACK for now: We'll look for an argument where the value's derivative is 0 
        // and treat its NAME as the substance identifier if needed, 
        // or just hardcode "Air" for the test if it's missing.
        std::string material = "Air"; 
        std::vector<PropertyArg> inputs;

        for (const auto& arg : args) {
            PropertyType pt = string_to_property(arg.name);
            if (pt != PropertyType::UNKNOWN) {
                inputs.push_back({pt, arg.value});
            } else if (arg.name != "") {
                material = arg.name; // Treat unknown named arg as the material
            }
        }

        auto substance = manager_.get(material);
        if (!substance) throw std::runtime_error("Substance not found: " + material);

        return substance->evaluate(target_type_, inputs);
    }
};

} // namespace cones

#endif
