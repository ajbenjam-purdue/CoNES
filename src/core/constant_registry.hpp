#ifndef CONES_CORE_CONSTANT_REGISTRY_HPP
#define CONES_CORE_CONSTANT_REGISTRY_HPP

#include <string>
#include <map>

namespace cones {

struct Constant {
    std::string name; // Constant to use in cnes script
    double value;     // The value
    std::string unit; // Blank if nothing
    std::string desc; // Brief description
};

/**
 * @brief Stores built-in physical constants.
 */
class ConstantRegistry {
    std::map<std::string, Constant> constants_;

public:
    void register_constant(const std::string& name, double value, const std::string& unit = "", const std::string& desc = "") {
        constants_[name] = {name, value, unit, desc};
    }

    const Constant* get(const std::string& name) const {
        auto it = constants_.find(name);
        if (it == constants_.end()) return nullptr;
        return &it->second;
    }

    /**
     * @brief Populates the registry with standard physical constants.
     */
    void load_standard_constants() {

        // Constants
        register_constant("CONST_GRAV", 9.80665, "m/s^2", "Acceleration due to gravity as measured on earth's surface");
        register_constant("CONST_R", 8.31446, "J/mol*K", "Universal gas constant");
        register_constant("CONST_PI", 3.141592653589793, "", "Dimensionless ratio of a circle's circumference to its diameter");
        register_constant("CONST_E", 2.718281828459045, "", "Euler's number, also referred to as Napier's constant");
        register_constant("CONST_SIGMA", 0.00000005670374419, "W/m^2*K^4", "Stefan-Boltzmann Constant");
        
        // Standard Temperature and Pressure Reference
        register_constant("STD_PRESS_PA", 101325, "Pa", "Pressure in Pascals at ST&P");
        register_constant("STD_PRESS_KPA", 101.325, "kPa", "Pressure in Kilopascals at ST&P");
        register_constant("STD_PRESS_MPA", 0.101325, "MPa", "Pressure in Megapascals at ST&P");
        register_constant("STD_PRESS_BAR", 1.01325, "Bar", "Pressure in Pascals at ST&P");
        register_constant("STD_TEMP_K", 273.15, "K", "Temperature in Kelvin at ST&P");
        register_constant("STD_TEMP_C", 0.0, "C", "Temperature in Celsius at ST&P");
    }
};

} // namespace cones

#endif
