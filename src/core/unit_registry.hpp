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
        register_unit("C", Unit::Coulomb(), "Coulomb [Electric Charge]");

        // Derived SI Units
        register_unit("N", Unit::Newton(), "Newton [Force]");
        register_unit("J", Unit::Joule(), "Joule [Energy]");
        register_unit("Pa", Unit::Pascal(), "Pascal [Pressure]");
        register_unit("W", Unit::Watt(), "Watt [Power]");
        register_unit("degC", Unit::Celsius(), "Celsius [Temperature]");
        register_unit("deg_C", Unit::Celsius(), "Celsius [Temperature]");

        // Electrical / Electromagnetic Units (EE)
        // Amperes
        register_unit("A", Unit::Ampere(), "Ampere [Electric Current]");
        register_unit("nA", Unit::Ampere() * 1e-9, "Nanoampere");
        register_unit("uA", Unit::Ampere() * 1e-6, "Microampere");
        register_unit("mA", Unit::Ampere() * 1e-3, "Milliampere");
        register_unit("kA", Unit::Ampere() * 1e3, "Kiloampere");

        // Volts
        register_unit("V", Unit::Volt(), "Volt [Electric Potential]");
        register_unit("nV", Unit::Volt() * 1e-9, "Nanovolt");
        register_unit("uV", Unit::Volt() * 1e-6, "Microvolt");
        register_unit("mV", Unit::Volt() * 1e-3, "Millivolt");
        register_unit("kV", Unit::Volt() * 1e3, "Kilovolt");

        // Ohms
        register_unit("Ohm", Unit::Ohm(), "Ohm [Electrical Resistance]");
        register_unit("uOhm", Unit::Ohm() * 1e-6, "Microohm");
        register_unit("mOhm", Unit::Ohm() * 1e-3, "Milliohm");
        register_unit("kOhm", Unit::Ohm() * 1e3, "Kiloohm");
        register_unit("MOhm", Unit::Ohm() * 1e6, "Megaohm");

        // Henries
        register_unit("H", Unit::Henry(), "Henry [Inductance]");
        register_unit("uH", Unit::Henry() * 1e-6, "Microhenry");
        register_unit("mH", Unit::Henry() * 1e-3, "Millihenry");
        register_unit("kH", Unit::Henry() * 1e3, "Kilohenry");

        // Hertz
        register_unit("Hz", Unit::Hertz(), "Hertz [Frequency]");
        register_unit("mHz", Unit::Hertz() * 1e-3, "Millihertz");
        register_unit("kHz", Unit::Hertz() * 1e3, "Kilohertz");
        register_unit("MHz", Unit::Hertz() * 1e6, "Megahertz");

        // Farads
        register_unit("F", Unit::Farad(), "Farad [Capacitance]");
        register_unit("pF", Unit::Farad() * 1e-12, "Picofarad");
        register_unit("nF", Unit::Farad() * 1e-9, "Nanofarad");
        register_unit("uF", Unit::Farad() * 1e-6, "Microfarad");
        register_unit("mF", Unit::Farad() * 1e-3, "Millifarad");
        register_unit("kF", Unit::Farad() * 1e3, "Kilofarad");

        // Webers
        register_unit("Wb", Unit::Weber(), "Weber [Magnetic Flux]");
        register_unit("pWb", Unit::Weber() * 1e-12, "Picoweber");
        register_unit("nWb", Unit::Weber() * 1e-9, "Nanoweber");
        register_unit("uWb", Unit::Weber() * 1e-6, "Micoweber");
        register_unit("mWb", Unit::Weber() * 1e-3, "Milliweber");

        // Gauss
        register_unit("Ga", Unit::Gauss(), "Gauss [Magnetic Flux Density]");
        register_unit("mGa", Unit::Gauss() * 1e-3, "Milligauss");
        register_unit("kGa", Unit::Gauss() * 1e3, "Kilogauss");
        register_unit("MGa", Unit::Gauss() * 1e6, "Megagauss");

        // Teslas
        register_unit("T", Unit::Tesla(), "Tesla [Magnetic Flux Density]");
        register_unit("nT", Unit::Tesla() * 1e-9, "Nanotesla");
        register_unit("uT", Unit::Tesla() * 1e-6, "Microtesla");
        register_unit("mT", Unit::Tesla() * 1e-3, "Millitesla");
        
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
        register_unit("J/kg*K", {1.0, {0, 2, -2, -1, 0, 0}}, "Specific Entropy");
        register_unit("kJ/kg*K", {1000.0, {0, 2, -2, -1, 0, 0}}, "Specific Entropy (kilo)");
        register_unit("J/kg", {1.0, {0, 2, -2, 0, 0, 0}}, "Specific Enthalpy");
        register_unit("kJ/kg", {1000.0, {0, 2, -2, 0, 0, 0}}, "Specific Enthalpy (kilo)");
        register_unit("kg/s", Unit::Kilogram() / Unit::Second(), "Mass Flow Rate");
        register_unit("kg/hr", Unit::Kilogram() / Unit(3600.0, {0, 0, 1, 0, 0, 0}), "Mass Flow Rate (hourly)");
        register_unit("m/s", Unit::Meter() / Unit::Second(), "Velocity");
        register_unit("cm/s", {0.01, {0, 1, -1, 0, 0, 0}}, "Velocity (cm/s)");
        register_unit("mm/s", {0.001, {0, 1, -1, 0, 0, 0}}, "Velocity (mm/s)");
        register_unit("m/s^2", Unit::Meter() / (Unit::Second() * Unit::Second()), "Acceleration");
        register_unit("G", {9.81, {0, 1, -2, 0, 0, 0}}, "Gravitational Acceleration");
        
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
        register_unit("psig", {6894.75729, {1, -1, -2, 0, 0, 0}, 14.69595}, "Pounds per square inch gauge (relative to 1 atm) [Pressure]");

        // Energy & Power
        register_unit("BTU", Unit::Joule() * 1055.056, "British Thermal Unit [Energy]");
        register_unit("cal", Unit::Joule() * 4.184, "Calorie [Energy]");
        register_unit("hp", Unit::Watt() * 745.69987, "Horsepower [Power]");
        
        // Temperature
        register_unit("F", {5.0/9.0, {0, 0, 0, 1, 0, 0}, 459.67}, "Fahrenheit [Temperature]");
        register_unit("R", {5.0/9.0, {0, 0, 0, 1, 0, 0}, 0.0}, "Rankine [Temperature]");
    }
};

} // namespace cones

#endif
