#ifndef CONES_CORE_UNIT_HPP
#define CONES_CORE_UNIT_HPP

#include <string>
#include <map>
#include <cmath>
#include <vector>
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace cones
{

    /**
     * @brief Dimensions: [Mass, Length, Time, Temp, Moles, Charge]
     */
    struct Unit
    {
        double scale = 1.0;                         // Scale relative to the SI reference
        double offset = 0.0;                        // Offset relative to the SI reference (temp)
        std::vector<int> dims = {0, 0, 0, 0, 0, 0}; // Tracks the relative power of each dimension [Mass, Length, Time, Temp, Moles, Charge]

        Unit() = default;                                                                               // Empty constructor
        Unit(double s, std::vector<int> d, double o = 0.0) : scale(s), offset(o), dims(std::move(d)) {} // Pre-pop constructor

        bool is_dimensionless() const // Returns true if the unit represents a dimensionless number
        {
            for (int d : dims)
                if (d != 0)
                    return false;
            return true;
        }
        bool requires_positivity() const { // Returns true if the unit has to be +ive (Temperature, Pressure, Density, Specific Volume, Viscosity)
            // Temperature [0,0,0,1,0,0]
            if (dims == std::vector<int>{0, 0, 0, 1, 0, 0}) return true;
            // Pressure [1,-1,-2,0,0,0]
            if (dims == std::vector<int>{1, -1, -2, 0, 0, 0}) return true;
            // Density [1,-3,0,0,0,0]
            if (dims == std::vector<int>{1, -3, 0, 0, 0, 0}) return true;
            // Specific Volume [-1,3,0,0,0,0]
            if (dims == std::vector<int>{-1, 3, 0, 0, 0, 0}) return true;
            // Viscosity [1,-1,-1,0,0,0]
            if (dims == std::vector<int>{1, -1, -1, 0, 0, 0}) return true;
            return false;
        }
        bool compatible(const Unit &other) const { return dims == other.dims; } // Are this unit and the other compatible (e.g. for addition/similar)?

        // Returns the BASE unit in SI representation (1 scale, 0 offset, same dims)
        // TODO: Review. Current naming is misleading
        Unit to_si() const { return {1.0, dims, 0.0}; }

        // Overloading

        Unit operator*(const Unit &other) const
        {
            std::vector<int> new_dims = dims;
            for (size_t i = 0; i < dims.size(); ++i)
                new_dims[i] += other.dims[i];
            return {scale * other.scale, new_dims, 0.0}; // TODO: Review impact of offsets
        }
        Unit operator*(double scalar) const
        {
            return {scale * scalar, dims, offset};
        }
        Unit operator/(const Unit &other) const
        {
            std::vector<int> new_dims = dims;
            for (size_t i = 0; i < dims.size(); ++i)
                new_dims[i] -= other.dims[i];
            return {scale / other.scale, new_dims, 0.0}; // Same issues as *
        }
        Unit pow(double p) const
        {
            std::vector<int> new_dims = dims;
            for (size_t i = 0; i < dims.size(); ++i)
                new_dims[i] = static_cast<int>(std::round(dims[i] * p));
            return {std::pow(scale, p), new_dims, offset};
        }

        // Member method
        std::string to_string() const
        {
            if (is_dimensionless()) return "";

            if (dims == std::vector<int>{1, 2, -2, 0, 0, 0}) // Energy
            {
                if (std::abs(scale - 1e3) < 1e-5)
                    return "kJ";
                if (std::abs(scale - 1e6) < 1e-5)
                    return "MJ";
                if (std::abs(scale - 1e9) < 1e-5)
                    return "GJ";
                return "J";
            }
            if (dims == std::vector<int>{1, 1, -2, 0, 0, 0}) // Force
            {
                if (std::abs(scale - 1e3) < 1e-5)
                    return "kN";
                if (std::abs(scale - 1e6) < 1e-5)
                    return "MN";
                return "N";
            }
            if (dims == std::vector<int>{0, 2, -2, 0, 0, 0}) // Entropy
            {
                return (std::abs(scale - 1000.0) < 1e-5) ? "kJ/kg" : "J/kg";
            }
            if (dims == std::vector<int>{0, 2, -2, -1, 0, 0}) // Enthalpy
            {
                return (std::abs(scale - 1000.0) < 1e-5) ? "kJ/kg*K" : "J/kg*K";
            }
            if (dims == std::vector<int>{1, 2, -3, 0, 0, 0}) // Power
            {
                if (std::abs(scale - 1e3) < 1e-5)
                    return "kW";
                if (std::abs(scale - 1e6) < 1e-5)
                    return "MW";
                return "W";
            }
            if (dims == std::vector<int>{1, -1, -2, 0, 0, 0}) // Pressure
            {
                if (std::abs(scale - 1e6) < 1e-5)
                    return "MPa";
                if (std::abs(scale - 1000.0) < 1e-5)
                    return "kPa";
                if (std::abs(scale - 1e5) < 1e-5)
                    return "bar";
                if (std::abs(scale - 100.0) < 1e-5)
                    return "mbar";
                return "Pa";
            }
            if (dims == std::vector<int>{0, 1, -1, 0, 0, 0}) // Speed
            {
                if (std::abs(scale - 0.01) < 1e-5)
                    return "cm/s";
                if (std::abs(scale - 0.001) < 1e-5)
                    return "mm/s";
                return "m/s";
            }
            if (dims == std::vector<int>{0, 1, -2, 0, 0, 0}) // Acceleration
            {
                if (std::abs(scale - 9.81) < 1e-5)
                    return "G";
                return "m/s^2";
            }
            if (dims == std::vector<int>{0, 0, 0, 1, 0, 0}) // Temperature
                return offset > 200 ? "degC" : "K";

            if (dims == std::vector<int>{0, 0, 0, 0, 0, 1}) // Coulomb
                return "C";

            if (dims == std::vector<int>{0, 0, -1, 0, 0, 1}) // Ampere
            {
                if (std::abs(scale - 1e-9) < 1e-12) return "nA";
                if (std::abs(scale - 1e-6) < 1e-9)  return "uA";
                if (std::abs(scale - 1e-3) < 1e-6)  return "mA";
                if (std::abs(scale - 1e3)  < 1e-1)  return "kA";
                return "A";
            }
            if (dims == std::vector<int>{1, 2, -2, 0, 0, -1}) // Volt
            {
                if (std::abs(scale - 1e-9) < 1e-12) return "nV";
                if (std::abs(scale - 1e-6) < 1e-9)  return "uV";
                if (std::abs(scale - 1e-3) < 1e-6)  return "mV";
                if (std::abs(scale - 1e3)  < 1e-1)  return "kV";
                return "V";
            }
            if (dims == std::vector<int>{1, 2, -1, 0, 0, -2}) // Ohm
            {
                if (std::abs(scale - 1e-6) < 1e-9)  return "uOhm";
                if (std::abs(scale - 1e-3) < 1e-6)  return "mOhm";
                if (std::abs(scale - 1e3)  < 1e-1)  return "kOhm";
                if (std::abs(scale - 1e6)  < 1e2)   return "MOhm";
                return "Ohm";
            }
            if (dims == std::vector<int>{1, 2, 0, 0, 0, -2}) // Henry
            {
                if (std::abs(scale - 1e-6) < 1e-9)  return "uH";
                if (std::abs(scale - 1e-3) < 1e-6)  return "mH";
                if (std::abs(scale - 1e3)  < 1e-1)  return "kH";
                return "H";
            }
            if (dims == std::vector<int>{-1, -2, 2, 0, 0, 2}) // Farad
            {
                if (std::abs(scale - 1e-12) < 1e-15) return "pF";
                if (std::abs(scale - 1e-9)  < 1e-12) return "nF";
                if (std::abs(scale - 1e-6)  < 1e-9)  return "uF";
                if (std::abs(scale - 1e-3)  < 1e-6)  return "mF";
                if (std::abs(scale - 1e3)   < 1e-1)  return "kF";
                return "F";
            }
            if (dims == std::vector<int>{1, 2, -1, 0, 0, -1}) // Weber
            {
                if (std::abs(scale - 1e-12) < 1e-15) return "pWb";
                if (std::abs(scale - 1e-9)  < 1e-12) return "nWb";
                if (std::abs(scale - 1e-6)  < 1e-9)  return "uWb";
                if (std::abs(scale - 1e-3)  < 1e-6)  return "mWb";
                return "Wb";
            }
            if (dims == std::vector<int>{1, 0, -1, 0, 0, -1}) // Tesla / Gauss
            {
                if (std::abs(scale - 1e-9) < 1e-12) return "nT";
                if (std::abs(scale - 1e-6) < 1e-9)  return "uT";
                if (std::abs(scale - 1e-3) < 1e-6)  return "mT";
                if (std::abs(scale - 1e-7) < 1e-10) return "mGa";
                if (std::abs(scale - 1e-4) < 1e-7)  return "Ga";
                if (std::abs(scale - 0.1)  < 1e-4)  return "kGa";
                if (std::abs(scale - 100.0)< 1e-1)  return "MGa";
                return "T";
            }

            if (dims == std::vector<int>{0, 0, 1, 0, 0, 0}) // Time
            {
                if (std::abs(scale - 60.0) < 1e-5)
                    return "min";
                if (std::abs(scale - 3600.0) < 1e-5)
                    return "hr";
                if (std::abs(scale - 0.001) < 1e-5)
                    return "ms";
                if (std::abs(scale - 0.000001) < 1e-8)
                    return "us";
            }

            if (dims == std::vector<int>{1, 0, -1, 0, 0, 0}) // Mass flow
            {
                return (std::abs(scale - 1.0 / 3600.0) < 1e-8) ? "kg/hr" : "kg/s";
            }
            if (dims == std::vector<int>{0, 1, 0, 0, 0, 0}) // Distance
            {
                if (std::abs(scale - 1000.0) < 1e-5)
                    return "km";
                if (std::abs(scale - 0.01) < 1e-5)
                    return "cm";
                if (std::abs(scale - 0.001) < 1e-5)
                    return "mm";
                return "m";
            }

            static const std::vector<std::string> names = {"kg", "m", "s", "K", "mol", "C"};
            std::string s = "";
            bool first = true;
            for (size_t i = 0; i < dims.size(); ++i)
            {
                if (dims[i] != 0)
                {
                    if (!first)
                        s += "*";
                    s += names[i];
                    if (dims[i] != 1)
                        s += "^" + std::to_string(dims[i]);
                    first = false;
                }
            }
            return s;
        }

        // Core and common units
        static Unit Dimensionless() { return {1.0, {0, 0, 0, 0, 0, 0}}; }
        static Unit Meter() { return {1.0, {0, 1, 0, 0, 0, 0}}; }
        static Unit Second() { return {1.0, {0, 0, 1, 0, 0, 0}}; }
        static Unit Kilogram() { return {1.0, {1, 0, 0, 0, 0, 0}}; }
        static Unit Kelvin() { return {1.0, {0, 0, 0, 1, 0, 0}}; }
        static Unit Celsius() { return {1.0, {0, 0, 0, 1, 0, 0}, 273.15}; }
        static Unit Newton() { return {1.0, {1, 1, -2, 0, 0, 0}}; }
        static Unit Joule() { return {1.0, {1, 2, -2, 0, 0, 0}}; }
        static Unit Pascal() { return {1.0, {1, -1, -2, 0, 0, 0}}; }
        static Unit Watt() { return {1.0, {1, 2, -3, 0, 0, 0}}; }
        static Unit Mol() { return {1.0, {0, 0, 0, 0, 1, 0}}; }
        static Unit Coulomb() { return {1.0, {0, 0, 0, 0, 0, 1}}; }
        static Unit Ampere() { return Coulomb() / Second(); }
        static Unit Volt() { return Joule() / Coulomb(); }
        static Unit Ohm() { return Volt() / Ampere(); }
        static Unit Farad() { return Coulomb() / Volt(); }
        static Unit Weber() { return Volt() * Second(); }
        static Unit Tesla() { return Weber() / (Meter() * Meter()); }
        static Unit Henry() { return Weber() / Ampere(); }
        static Unit Hertz() { return Dimensionless() / Second(); }
        static Unit Gauss() { return Tesla() * 1e-4; }
    };

} // namespace cones

namespace std
{
    // Equivalent to u.to_string()
    inline std::string to_string(const cones::Unit &u)
    {
        return u.to_string();
    }
} // namespace std

#endif
