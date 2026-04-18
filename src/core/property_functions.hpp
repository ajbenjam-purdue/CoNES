#ifndef CONES_CORE_PROPERTY_FUNCTIONS_HPP
#define CONES_CORE_PROPERTY_FUNCTIONS_HPP

#include "function_registry.hpp"
#include "substance_manager.hpp"
#include <iostream>
#include <cmath>

namespace cones {

inline PropertyType unit_to_property(const Unit& u) {
    if (u.dims == std::vector<int>{0, 0, 0, 1, 0}) return PropertyType::TEMPERATURE;
    if (u.dims == std::vector<int>{1, -1, -2, 0, 0}) return PropertyType::PRESSURE;
    if (u.dims == std::vector<int>{1, 2, -2, 0, 0}) return PropertyType::ENTHALPY;
    if (u.dims == std::vector<int>{0, 0, -2, 0, 0}) return PropertyType::ENTHALPY; // J/kg but no mass dim? Check.
    if (u.dims == std::vector<int>{1, -3, 0, 0, 0}) return PropertyType::DENSITY;
    return PropertyType::UNKNOWN;
}

class MathFunction : public IFunction {
    std::string name_, args_, desc_;
public:
    MathFunction(std::string n, std::string a, std::string d) : name_(n), args_(a), desc_(d) {}
    std::string name() const override { return name_; }
    std::string args_metadata() const override { return args_; }
    std::string description() const override { return desc_; }
    void validate(const std::vector<FuncArg>& args) const override {
        if (args.size() != 1) throw std::runtime_error(name_ + "() requires exactly 1 argument.");
    }
    DualNumber evaluate(const std::vector<FuncArg>& args) const override {
        validate(args);
        double v = args[0].value.val;
        double d = args[0].value.der;
        if (name_ == "sin") return { std::sin(v), std::cos(v) * d };
        if (name_ == "cos") return { std::cos(v), -std::sin(v) * d };
        if (name_ == "tan") return { std::tan(v), (1.0 / std::pow(std::cos(v), 2)) * d };
        if (name_ == "sqrt") return { std::sqrt(v), (0.5 / std::sqrt(v)) * d };
        if (name_ == "log") return { std::log(v), (1.0 / v) * d };
        if (name_ == "exp") return { std::exp(v), std::exp(v) * d };
        throw std::runtime_error("Unknown math function: " + name_);
    }
    Unit get_unit(const std::vector<Unit>& input_units) const override {
        if (input_units.empty()) return Unit::Dimensionless();
        if (name_ == "sqrt") return input_units[0].pow(0.5);
        return Unit::Dimensionless();
    }
};

class GeneralPropertyFunction : public IFunction {
    std::string name_, desc_;
    PropertyType target_type_;
    const SubstanceManager& manager_;
public:
    GeneralPropertyFunction(std::string n, PropertyType t, const SubstanceManager& m, std::string d = "")
        : name_(n), target_type_(t), manager_(m), desc_(d) {}

    std::string name() const override { return name_; }
    std::string args_metadata() const override { return "Material, Two independent properties"; }
    std::string description() const override { return desc_; }
    void validate(const std::vector<FuncArg>& args) const override {}

    DualNumber evaluate(const std::vector<FuncArg>& args) const override {
        std::string material = "Air";
        std::vector<PropertyArg> inputs;

        for (const auto& arg : args) {
            PropertyType pt = PropertyType::UNKNOWN;
            
            // 1. Explicit naming
            if (!arg.name.empty()) {
                pt = string_to_property(arg.name);
                if (pt == PropertyType::UNKNOWN) material = arg.name;
            }
            
            // 2. Unit Inference
            if (pt == PropertyType::UNKNOWN && !arg.unit.is_dimensionless()) {
                pt = unit_to_property(arg.unit);
            }

            if (pt != PropertyType::UNKNOWN) {
                // Convert to SI (Standard internal units)
                double si_val = (arg.value.val + arg.unit.offset) * arg.unit.scale;
                double si_der = arg.value.der * arg.unit.scale;
                inputs.push_back({pt, {si_val, si_der}});
            }
        }

        auto substance = manager_.get(material);
        if (!substance) throw std::runtime_error("Substance not found: " + material);
        return substance->evaluate(target_type_, inputs);
    }

    Unit get_unit(const std::vector<Unit>&) const override {
        if (target_type_ == PropertyType::PRESSURE) return Unit::Pascal();
        if (target_type_ == PropertyType::TEMPERATURE) return Unit::Kelvin();
        if (target_type_ == PropertyType::ENTHALPY) return {1.0, {1, 2, -2, 0, 0}}; // Joule/kg? Dims are complex.
        return Unit::Dimensionless();
    }
};

} // namespace cones

#endif
