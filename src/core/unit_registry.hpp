#ifndef CONES_CORE_UNIT_REGISTRY_HPP
#define CONES_CORE_UNIT_REGISTRY_HPP

#include "unit.hpp"
#include <string>
#include <map>
#include <vector>
#include <memory>

namespace cones {

struct UnitDefinition {
    std::string name;
    Unit unit;
    std::string description;
};

class UnitRegistry {
    std::map<std::string, UnitDefinition> units_;

public:
    UnitRegistry() {
        load_defaults();
    }

    void register_unit(const std::string& name, const Unit& unit, const std::string& desc = "") {
        units_[name] = {name, unit, desc};
    }

    const UnitDefinition* get(const std::string& name) const {
        auto it = units_.find(name);
        if (it != units_.end()) return &it->second;
        return nullptr;
    }

    std::vector<std::string> get_all_names() const {
        std::vector<std::string> names;
        for (const auto& [name, _] : units_) names.push_back(name);
        return names;
    }

private:
    void load_defaults() {
        // SI UNITS
        // Base SI Units
        register_unit("m", Unit::Meter(), "Meter [Length]");
        register_unit("s", Unit::Second(), "Second [Time]");
        register_unit("kg", Unit::Kilogram(), "Kilogram [Mass]");
        register_unit("K", Unit::Kelvin(), "Kelvin [Temperature]");
        register_unit("mol", Unit::Mol(), "Mole [Amount of substance]");

        // Derived SI Units
        register_unit("N", Unit::Newton(), "Newton [Force]");
        register_unit("J", Unit::Joule(), "Joule [Energy]");
        register_unit("Pa", Unit::Pascal(), "Pascal [Pressure]");
        register_unit("W", Unit::Watt(), "Watt [Power]");
        register_unit("C", Unit::Celsius(), "Celsius [Temperature]");
        
        // SI Prefixes & Common variations
        register_unit("kJ", Unit::Joule() * 1000.0, "Kilojoule");
        register_unit("MJ", Unit::Joule() * 1e6, "Megajoule");
        register_unit("GJ", Unit::Joule() * 1e9, "Gigajoule");
        register_unit("kN", Unit::Newton() * 1000.0, "Kilonewton");
        register_unit("MN", Unit::Newton() * 1e6, "Meganewton");
        register_unit("kW", Unit::Watt() * 1000.0, "Kilowatt");
        register_unit("MW", Unit::Watt() * 1e6, "Megawatt");
        register_unit("km", Unit::Meter() * 1000.0, "Kilometer");
        register_unit("mm", Unit::Meter() * 0.001, "Millimeter");
        register_unit("cm", Unit::Meter() * 0.01, "Centimeter");
        register_unit("ms", Unit::Second() * 0.001, "Millisecond");
        register_unit("us", Unit::Second() * 0.000001, "Microsecond");
        register_unit("min", Unit::Second() * 60.0, "Minute");
        register_unit("hr", Unit::Second() * 3600.0, "Hour");
        register_unit("kPa", Unit::Pascal() * 1000.0, "Kilopascal");
        register_unit("MPa", Unit::Pascal() * 1e6, "Megapascal");
        register_unit("bar", Unit::Pascal() * 1e5, "Bar [Pressure]");
        register_unit("mbar", Unit::Pascal() * 100.0, "Millibar [Pressure]");
        register_unit("atm", Unit::Pascal() * 101325.0, "Standard Atmosphere [Pressure]");

        // Eng Units
        register_unit("J/kg*K", {1.0, {0, 2, -2, -1, 0}}, "Specific Entropy");
        register_unit("kJ/kg*K", {1000.0, {0, 2, -2, -1, 0}}, "Specific Entropy (kilo)");
        register_unit("J/kg", {1.0, {0, 2, -2, 0, 0}}, "Specific Enthalpy");
        register_unit("kJ/kg", {1000.0, {0, 2, -2, 0, 0}}, "Specific Enthalpy (kilo)");
        register_unit("kg/s", Unit::Kilogram() / Unit::Second(), "Mass Flow Rate");
        register_unit("kg/hr", Unit::Kilogram() / Unit(3600.0, {0, 0, 1, 0, 0}), "Mass Flow Rate (hourly)");
        register_unit("m/s", Unit::Meter() / Unit::Second(), "Velocity");
        register_unit("cm/s", {0.01, {0, 1, -1, 0, 0}}, "Velocity (cm/s)");
        register_unit("mm/s", {0.001, {0, 1, -1, 0, 0}}, "Velocity (mm/s)");
        register_unit("m/s^2", Unit::Meter() / (Unit::Second() * Unit::Second()), "Acceleration");
        register_unit("G", {9.81, {0, 1, -2, 0, 0}}, "Gravitational Acceleration");
        
        // US CUSTOMARY / IMPERIAL UNITS
        // Length
        register_unit("ft", Unit::Meter() * 0.3048, "Foot [Length]");
        register_unit("in", Unit::Meter() * 0.0254, "Inch [Length]");
        register_unit("mile", Unit::Meter() * 1609.344, "Mile [Length]");
        
        // Mass / Force
        register_unit("lbm", Unit::Kilogram() * 0.4535924, "Pound-mass [Mass]");
        register_unit("slg", Unit::Kilogram() * 0.0685218, "Slug [Mass]");
        register_unit("lbf", Unit::Newton() * 4.4482216153, "Pound-force [Force]");
        
        // Pressure
        register_unit("psia", Unit::Pascal() * 6894.75729, "Pounds per square inch absolute [Pressure]");
        register_unit("psig", {6894.75729, {1, -1, -2, 0, 0}, 14.69595}, "Pounds per square inch gauge (relative to 1 atm) [Pressure]");

        // Energy & Power
        register_unit("BTU", Unit::Joule() * 1055.056, "British Thermal Unit [Energy]");
        register_unit("cal", Unit::Joule() * 4.184, "Calorie [Energy]");
        register_unit("hp", Unit::Watt() * 745.69987, "Horsepower [Power]");
        
        // Temperature
        register_unit("F", {5.0/9.0, {0, 0, 0, 1, 0}, 459.67}, "Fahrenheit [Temperature]");
        register_unit("R", {5.0/9.0, {0, 0, 0, 1, 0}, 0.0}, "Rankine [Temperature]");
    }
};

} // namespace cones

#endif
