#ifndef CONES_CORE_UNIT_HPP
#define CONES_CORE_UNIT_HPP

#include <string>
#include <map>
#include <cmath>
#include <vector>
#include <sstream>

namespace cones
{

    /**
     * @brief Dimensions: [Mass, Length, Time, Temp, Moles]
     */
    struct Unit
    {
        double scale = 1.0;
        double offset = 0.0;
        std::vector<int> dims = {0, 0, 0, 0, 0};

        Unit() = default;
        Unit(double s, std::vector<int> d, double o = 0.0) : scale(s), offset(o), dims(std::move(d)) {}

        bool is_dimensionless() const
        {
            for (int d : dims)
                if (d != 0)
                    return false;
            return true;
        }
        bool compatible(const Unit &other) const { return dims == other.dims; }

        Unit to_si() const { return {1.0, dims, 0.0}; }

        Unit operator*(const Unit &other) const
        {
            std::vector<int> new_dims = dims;
            for (size_t i = 0; i < dims.size(); ++i)
                new_dims[i] += other.dims[i];
            return {scale * other.scale, new_dims, 0.0};
        }
        Unit operator/(const Unit &other) const
        {
            std::vector<int> new_dims = dims;
            for (size_t i = 0; i < dims.size(); ++i)
                new_dims[i] -= other.dims[i];
            return {scale / other.scale, new_dims, 0.0};
        }
        Unit pow(double p) const
        {
            std::vector<int> new_dims = dims;
            for (int &d : new_dims)
                d = static_cast<int>(d * p);
            return {std::pow(scale, p), new_dims, offset};
        }

        std::string to_string() const
        {
            if (is_dimensionless())
                return "";

            // Energy check: M*L^2/T^2
            if (dims == std::vector<int>{1, 2, -2, 0, 0})
                return "J";
            if (dims == std::vector<int>{1, 1, -2, 0, 0})
                return "N";
            if (dims == std::vector<int>{0, 2, -2, 0, 0})
            {
                return (std::abs(scale - 1000.0) < 1e-5) ? "kJ/kg" : "J/kg";
            }
            if (dims == std::vector<int>{0, 2, -2, -1, 0})
            {
                return (std::abs(scale - 1000.0) < 1e-5) ? "kJ/kg*K" : "J/kg*K";
            }
            if (dims == std::vector<int>{1, 2, -3, 0, 0})
            {
                return (std::abs(scale - 1000.0) < 1e-5) ? "kW" : "W";
            }
            if (dims == std::vector<int>{1, -1, -2, 0, 0})
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
            if (dims == std::vector<int>{0, 1, -1, 0, 0})
            {
                if (std::abs(scale - 0.01) < 1e-5)
                    return "cm/s";
                if (std::abs(scale - 0.001) < 1e-5)
                    return "mm/s";
                return "m/s";
            }
            if (dims == std::vector<int>{0, 1, -2, 0, 0})
            {
                if (std::abs(scale - 9.81) < 1e-5)
                    return "G";
                return "m/s^2";
            }
            if (dims == std::vector<int>{0, 0, 0, 1, 0})
                return offset > 200 ? "C" : "K";

            if (dims == std::vector<int>{0, 0, 1, 0, 0})
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

            if (dims == std::vector<int>{0, 1, 0, 0, 0})
            {
                if (std::abs(scale - 1000.0) < 1e-5)
                    return "km";
                if (std::abs(scale - 0.01) < 1e-5)
                    return "cm";
                if (std::abs(scale - 0.001) < 1e-5)
                    return "mm";
            }

            static const std::vector<std::string> names = {"kg", "m", "s", "K", "mol"};
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
        static Unit Dimensionless() { return {1.0, {0, 0, 0, 0, 0}}; }
        static Unit Meter() { return {1.0, {0, 1, 0, 0, 0}}; }
        static Unit Second() { return {1.0, {0, 0, 1, 0, 0}}; }
        static Unit Kilogram() { return {1.0, {1, 0, 0, 0, 0}}; }
        static Unit Kelvin() { return {1.0, {0, 0, 0, 1, 0}}; }
        static Unit Celsius() { return {1.0, {0, 0, 0, 1, 0}, 273.15}; }
        static Unit Newton() { return {1.0, {1, 1, -2, 0, 0}}; }
        static Unit Joule() { return {1.0, {1, 2, -2, 0, 0}}; }
        static Unit Pascal() { return {1.0, {1, -1, -2, 0, 0}}; }

        // Inference
        static Unit from_string(const std::string &s)
        {
            if (s == "m")
                return Meter();
            if (s == "s")
                return Second();
            if (s == "kg")
                return Kilogram();
            if (s == "K")
                return Kelvin();
            if (s == "C")
                return Celsius();
            if (s == "N")
                return Newton();
            if (s == "J")
                return Joule();
            if (s == "Pa")
                return Pascal();
            if (s == "W")
                return {1.0, {1, 2, -3, 0, 0}};
            if (s == "kW")
                return {1000.0, {1, 2, -3, 0, 0}};
            if (s == "mol")
                return {1.0, {0, 0, 0, 0, 1}};
            if (s == "km")
                return {1000.0, {0, 1, 0, 0, 0}};
            if (s == "mm")
                return {0.001, {0, 1, 0, 0, 0}};
            if (s == "cm")
                return {0.01, {0, 1, 0, 0, 0}};
            if (s == "ms")
                return {0.001, {0, 0, 1, 0, 0}};
            if (s == "us")
                return {0.000001, {0, 0, 1, 0, 0}};
            if (s == "min")
                return {60, {0, 0, 1, 0, 0}};
            if (s == "hr")
                return {3600, {0, 0, 1, 0, 0}};
            if (s == "kPa")
                return {1000.0, {1, -1, -2, 0, 0}};
            if (s == "MPa")
                return {1e6, {1, -1, -2, 0, 0}};
            if (s == "bar" || s == "Bar")
                return {1e5, {1, -1, -2, 0, 0}};
            if (s == "mbar" || s == "mBar")
                return {100, {1, -1, -2, 0, 0}};
            if (s == "J/kg*K")
                return {1.0, {0, 2, -2, -1, 0}};
            if (s == "kJ/kg*K")
                return {1000.0, {0, 2, -2, -1, 0}};
            if (s == "J/kg")
                return {1.0, {0, 2, -2, 0, 0}};
            if (s == "kJ/kg")
                return {1000.0, {0, 2, -2, 0, 0}};
            if (s == "m/s")
                return Meter() / Second();
            if (s == "cm/s")
                return {0.01, {0, 1, -1, 0, 0}};
            if (s == "mm/s")
                return {0.001, {0, 1, -1, 0, 0}};
            if (s == "m/s^2")
                return Meter() / (Second() * Second());
            if (s == "G")
                return {9.81, {0, 1, -2, 0, 0}};

            // None found
            return Dimensionless();
        }
    };

} // namespace cones

#endif
