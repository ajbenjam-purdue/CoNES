#ifndef CONES_CORE_PROPERTY_FUNCTIONS_HPP
#define CONES_CORE_PROPERTY_FUNCTIONS_HPP

#include "function_registry.hpp"
#include "substance_manager.hpp"
#include <iostream>
#include <cmath>
#include <map>
#include <string>

namespace cones {

inline PropertyType unit_to_property(const Unit& u) {
    if (u.dims == std::vector<int>{0, 0, 0, 1, 0}) return PropertyType::TEMPERATURE;
    if (u.dims == std::vector<int>{1, -1, -2, 0, 0}) return PropertyType::PRESSURE;
    if (u.dims == std::vector<int>{1, 2, -2, 0, 0}) return PropertyType::ENTHALPY; // Joule (Total)
    if (u.dims == std::vector<int>{0, 2, -2, 0, 0}) return PropertyType::ENTHALPY; // Specific (J/kg)
    if (u.dims == std::vector<int>{0, 2, -2, -1, 0}) return PropertyType::ENTROPY; // J/kg*K
    if (u.dims == std::vector<int>{1, -3, 0, 0, 0}) return PropertyType::DENSITY;
    if (u.dims == std::vector<int>{-1, 3, 0, 0, 0}) return PropertyType::SPECIFIC_VOLUME;
    if (u.dims == std::vector<int>{1, -1, -1, 0, 0}) return PropertyType::VISCOSITY; // Pa*s
    if (u.dims == std::vector<int>{1, 1, -3, -1, 0}) return PropertyType::CONDUCTIVITY; // W/m*K
    return PropertyType::UNKNOWN;
}

struct FuncDef {
    std::string args;
    std::string desc;
    int num_args;
    std::function<DualNumber(const std::vector<DualNumber>&)> eval;
    std::function<Unit(const std::vector<Unit>&)> unit_transform;
};

inline const std::map<std::string, FuncDef>& get_builtin_math_functions() {
    static const std::map<std::string, FuncDef> funcs = {
        {"sin", {"x", "Trigonometric sine (radians)", 1, [](const std::vector<DualNumber>& a) { return DualNumber(std::sin(a[0].val), std::cos(a[0].val) * a[0].der); }, [](const std::vector<Unit>&) { return Unit::Dimensionless(); }}},
        {"cos", {"x", "Trigonometric cosine (radians)", 1, [](const std::vector<DualNumber>& a) { return DualNumber(std::cos(a[0].val), -std::sin(a[0].val) * a[0].der); }, [](const std::vector<Unit>&) { return Unit::Dimensionless(); }}},
        {"tan", {"x", "Trigonometric tangent (radians)", 1, [](const std::vector<DualNumber>& a) { return DualNumber(std::tan(a[0].val), (1.0 / std::pow(std::cos(a[0].val), 2)) * a[0].der); }, [](const std::vector<Unit>&) { return Unit::Dimensionless(); }}},
        {"asin", {"x", "Inverse sine", 1, [](const std::vector<DualNumber>& a) { return DualNumber(std::asin(a[0].val), a[0].der / std::sqrt(1.0 - a[0].val*a[0].val)); }, [](const std::vector<Unit>&) { return Unit::Dimensionless(); }}},
        {"acos", {"x", "Inverse cosine", 1, [](const std::vector<DualNumber>& a) { return DualNumber(std::acos(a[0].val), -a[0].der / std::sqrt(1.0 - a[0].val*a[0].val)); }, [](const std::vector<Unit>&) { return Unit::Dimensionless(); }}},
        {"atan", {"x", "Inverse tangent", 1, [](const std::vector<DualNumber>& a) { return DualNumber(std::atan(a[0].val), a[0].der / (1.0 + a[0].val*a[0].val)); }, [](const std::vector<Unit>&) { return Unit::Dimensionless(); }}},
        {"sinh", {"x", "Hyperbolic sine", 1, [](const std::vector<DualNumber>& a) { return DualNumber(std::sinh(a[0].val), std::cosh(a[0].val) * a[0].der); }, [](const std::vector<Unit>&) { return Unit::Dimensionless(); }}},
        {"cosh", {"x", "Hyperbolic cosine", 1, [](const std::vector<DualNumber>& a) { return DualNumber(std::cosh(a[0].val), std::sinh(a[0].val) * a[0].der); }, [](const std::vector<Unit>&) { return Unit::Dimensionless(); }}},
        {"tanh", {"x", "Hyperbolic tangent", 1, [](const std::vector<DualNumber>& a) { return DualNumber(std::tanh(a[0].val), (1.0 / std::pow(std::cosh(a[0].val), 2)) * a[0].der); }, [](const std::vector<Unit>&) { return Unit::Dimensionless(); }}},
        {"sqrt", {"x", "Square root", 1, [](const std::vector<DualNumber>& a) { return DualNumber(std::sqrt(a[0].val), (0.5 / std::sqrt(a[0].val)) * a[0].der); }, [](const std::vector<Unit>& u) { return u[0].pow(0.5); }}},
        {"log", {"x", "Natural logarithm (base-e)", 1, [](const std::vector<DualNumber>& a) { return DualNumber(std::log(a[0].val), a[0].der / a[0].val); }, [](const std::vector<Unit>&) { return Unit::Dimensionless(); }}},
        {"log10", {"x", "Common logarithm (base-10)", 1, [](const std::vector<DualNumber>& a) { return DualNumber(std::log10(a[0].val), a[0].der / (a[0].val * std::log(10.0))); }, [](const std::vector<Unit>&) { return Unit::Dimensionless(); }}},
        {"exp", {"x", "Exponential function (e^x)", 1, [](const std::vector<DualNumber>& a) { return DualNumber(std::exp(a[0].val), std::exp(a[0].val) * a[0].der); }, [](const std::vector<Unit>&) { return Unit::Dimensionless(); }}},
        {"abs", {"x", "Absolute value", 1, [](const std::vector<DualNumber>& a) { return DualNumber(std::abs(a[0].val), (a[0].val >= 0 ? 1.0 : -1.0) * a[0].der); }, [](const std::vector<Unit>& u) { return u[0]; }}},
        {"round", {"x", "Round to nearest integer", 1, [](const std::vector<DualNumber>& a) { return DualNumber(std::round(a[0].val), 0.0); }, [](const std::vector<Unit>& u) { return u[0]; }}},
        {"floor", {"x", "Floor function", 1, [](const std::vector<DualNumber>& a) { return DualNumber(std::floor(a[0].val), 0.0); }, [](const std::vector<Unit>& u) { return u[0]; }}},
        {"ceil", {"x", "Ceiling function", 1, [](const std::vector<DualNumber>& a) { return DualNumber(std::ceil(a[0].val), 0.0); }, [](const std::vector<Unit>& u) { return u[0]; }}}
    };
    return funcs;
}

inline const std::map<std::string, FuncDef>& get_builtin_physics_functions() {
    static const std::map<std::string, FuncDef> funcs = {
        {"Q_conv", {"h, A, Ts, Tinf", "Newton's Law of Cooling: Q = h*A*(Ts - Tinf)", 4, 
            [](const std::vector<DualNumber>& a) { 
                return a[0] * a[1] * (a[2] - a[3]); 
            }, 
            [](const std::vector<Unit>& u) { 
                return u[0] * u[1] * u[2]; 
            }
        }},
        {"Q_cond", {"k, A, dT, L", "Fourier's Law (simplified): Q = k*A*dT/L", 4, 
            [](const std::vector<DualNumber>& a) { 
                return (a[0] * a[1] * a[2]) / a[3]; 
            }, 
            [](const std::vector<Unit>& u) { 
                return (u[0] * u[1] * u[2]) / u[3]; 
            }
        }},
        {"Q_rad", {"eps, A, Ts, Tsur", "Stefan-Boltzmann Law: Q = eps*sigma*A*(Ts^4 - Tsur^4)", 4, 
            [](const std::vector<DualNumber>& a) { 
                const double sigma = 5.670374419e-8;
                return a[0] * sigma * a[1] * (a[2].pow(4) - a[3].pow(4)); 
            }, 
            [](const std::vector<Unit>&) { 
                return Unit(1.0, {1, 2, -3, 0, 0}); // Watts
            }
        }}

    };
    return funcs;
}

class GenericFunction : public IFunction {
    std::string name_;
    FuncDef def_;
public:
    GenericFunction(std::string name, FuncDef def) : name_(std::move(name)), def_(std::move(def)) {}
    std::string name() const override { return name_; }
    std::string args_metadata() const override { return def_.args; }
    std::string description() const override { return def_.desc; }
    void validate(const std::vector<FuncArg>& args) const override {
        if (def_.num_args >= 0 && (int)args.size() != def_.num_args) {
            throw std::runtime_error(name_ + "() requires exactly " + std::to_string(def_.num_args) + " arguments.");
        }
    }
    DualNumber evaluate(const std::vector<FuncArg>& args) const override {
        validate(args);
        std::vector<DualNumber> vals;
        for (const auto& a : args) vals.push_back(a.value);
        return def_.eval(vals);
    }
    Unit get_unit(const std::vector<Unit>& input_units) const override {
        return def_.unit_transform(input_units);
    }
};

class GeneralPropertyFunction : public IFunction {
    std::string name_, desc_;
    PropertyType target_type_;
    const SubstanceManager& manager_;
public:
    GeneralPropertyFunction(std::string n, PropertyType t, const SubstanceManager& m, std::string d = "")
        : name_(n), desc_(d), target_type_(t), manager_(m) {}

    std::string name() const override { return name_; }
    std::string args_metadata() const override { return "Material, Two independent properties"; }
    std::string description() const override { return desc_; }
    void validate(const std::vector<FuncArg>&) const override {}

    DualNumber evaluate(const std::vector<FuncArg>& args) const override {
        std::string material = "Air";
        std::vector<PropertyArg> inputs;

        for (const auto& arg : args) {
            PropertyType pt = PropertyType::UNKNOWN;
            
            // 1. Explicit naming or positional variable name
            if (!arg.name.empty()) {
                pt = string_to_property(arg.name);
                if (pt == PropertyType::UNKNOWN) {
                    // If it's not a property, check if it's a known substance
                    if (manager_.get(arg.name)) {
                        material = arg.name;
                    }
                }
            }
            
            // 2. Unit Inference (if pt is still unknown)
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
        switch (target_type_) {
            case PropertyType::PRESSURE:         return Unit::Pascal();
            case PropertyType::TEMPERATURE:      return Unit::Kelvin();
            case PropertyType::ENTHALPY:         return Unit(1.0, {0, 2, -2, 0, 0}); // J/kg
            case PropertyType::ENTROPY:          return Unit(1.0, {0, 2, -2, -1, 0}); // J/kg*K
            case PropertyType::INTERNAL_ENERGY:  return Unit(1.0, {0, 2, -2, 0, 0}); // J/kg
            case PropertyType::SPECIFIC_VOLUME:  return Unit(1.0, {-1, 3, 0, 0, 0}); // m^3/kg
            case PropertyType::DENSITY:          return Unit(1.0, {1, -3, 0, 0, 0}); // kg/m^3
            case PropertyType::QUALITY:          return Unit::Dimensionless();
            case PropertyType::VISCOSITY:        return Unit(1.0, {1, -1, -1, 0, 0}); // Pa*s = kg/m*s
            case PropertyType::CONDUCTIVITY:     return Unit(1.0, {1, 1, -3, -1, 0}); // W/m*K = kg*m/s^3*K
            case PropertyType::PRANDTL:          return Unit::Dimensionless();
            default:                             return Unit::Dimensionless();
        }
    }
};

inline void register_builtin_functions(FunctionRegistry& reg, const SubstanceManager& manager) {
    // Math functions
    for (const auto& [name, def] : get_builtin_math_functions()) {
        reg.register_function(std::make_shared<GenericFunction>(name, def));
    }

    // Physics functions
    for (const auto& [name, def] : get_builtin_physics_functions()) {
        reg.register_function(std::make_shared<GenericFunction>(name, def));
    }

    // Property functions
    auto add_prop = [&](std::string name, PropertyType type, std::string desc) {
        reg.register_function(std::make_shared<GeneralPropertyFunction>(name, type, manager, desc));
    };

    add_prop("Pressure",       PropertyType::PRESSURE,        "Yields the pressure in Pascals.");
    add_prop("Temperature",    PropertyType::TEMPERATURE,     "Yields the temperature in Kelvin.");
    add_prop("Enthalpy",       PropertyType::ENTHALPY,        "Yields the specific enthalpy in J/kg.");
    add_prop("Entropy",        PropertyType::ENTROPY,         "Yields the specific entropy in J/kg*K.");
    add_prop("InternalEnergy", PropertyType::INTERNAL_ENERGY, "Yields the specific internal energy in J/kg.");
    add_prop("SpecificVolume", PropertyType::SPECIFIC_VOLUME, "Yields the specific volume in m^3/kg.");
    add_prop("Density",        PropertyType::DENSITY,         "Yields the density in kg/m^3.");
    add_prop("Quality",        PropertyType::QUALITY,         "Yields the vapor quality.");
    add_prop("Viscosity",      PropertyType::VISCOSITY,       "Yields the dynamic viscosity in Pa*s.");
    add_prop("Conductivity",   PropertyType::CONDUCTIVITY,    "Yields the thermal conductivity in W/m*K.");
    add_prop("Prandtl",        PropertyType::PRANDTL,         "Yields the Prandtl number.");
}

} // namespace cones

#endif
