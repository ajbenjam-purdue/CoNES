#ifndef CONES_CORE_UNIT_HPP
#define CONES_CORE_UNIT_HPP

#include <string>
#include <map>
#include <cmath>
#include <vector>
#include <sstream>

namespace cones {

/**
 * @brief Represents a physical unit using base dimensions and a scaling factor.
 * Dimensions: [Mass, Length, Time, Temperature, Moles]
 */
struct Unit {
    double scale = 1.0;
    double offset = 0.0; // For Celsius/Kelvin conversion
    std::vector<int> dims = {0, 0, 0, 0, 0}; 

    Unit() = default;
    Unit(double s, std::vector<int> d, double o = 0.0) : scale(s), offset(o), dims(std::move(d)) {}

    bool is_dimensionless() const {
        for (int d : dims) if (d != 0) return false;
        return true;
    }

    bool compatible(const Unit& other) const {
        return dims == other.dims;
    }

    // Arithmetic for propagation
    Unit operator*(const Unit& other) const {
        std::vector<int> new_dims = dims;
        for (size_t i = 0; i < dims.size(); ++i) new_dims[i] += other.dims[i];
        return {scale * other.scale, new_dims, 0.0};
    }

    Unit operator/(const Unit& other) const {
        std::vector<int> new_dims = dims;
        for (size_t i = 0; i < dims.size(); ++i) new_dims[i] -= other.dims[i];
        return {scale / other.scale, new_dims, 0.0};
    }

    Unit pow(double p) const {
        std::vector<int> new_dims = dims;
        for (int& d : new_dims) d = static_cast<int>(d * p);
        return {std::pow(scale, p), new_dims, offset};
    }

    std::string to_string() const {
        if (is_dimensionless()) return "";
        
        // 1. Check for known aliases first (Dimensions: M, L, T, K, mol)
        if (dims == std::vector<int>{1, 1, -2, 0, 0}) return "N";
        if (dims == std::vector<int>{1, 2, -2, 0, 0}) return "J";
        if (dims == std::vector<int>{1, 2, -3, 0, 0}) {
            if (std::abs(scale - 1000.0) < 1e-5) return "kW";
            return "W";
        }
        if (dims == std::vector<int>{1, -1, -2, 0, 0}) return "Pa";
        if (dims == std::vector<int>{0, 1, -1, 0, 0}) return "m/s";
        if (dims == std::vector<int>{0, 1, -2, 0, 0}) return "m/s^2";
        if (dims == std::vector<int>{0, 0, 0, 1, 0}) {
            if (offset > 0) return "C";
            return "K";
        }

        // 2. Fallback to base dimensions
        static const std::vector<std::string> names = {"kg", "m", "s", "K", "mol"};
        std::string s = "";
        bool first = true;
        for (size_t i = 0; i < dims.size(); ++i) {
            if (dims[i] != 0) {
                if (!first) s += "*";
                s += names[i];
                if (dims[i] != 1) s += "^" + std::to_string(dims[i]);
                first = false;
            }
        }
        return s;
    }

    // Static helpers for common units
    static Unit Dimensionless() { return {1.0, {0,0,0,0,0}}; }
    static Unit Meter()  { return {1.0, {0,1,0,0,0}}; }
    static Unit Second() { return {1.0, {0,0,1,0,0}}; }
    static Unit Kilogram() { return {1.0, {1,0,0,0,0}}; }
    static Unit Kelvin() { return {1.0, {0,0,0,1,0}}; }
    static Unit Celsius() { return {1.0, {0,0,0,1,0}, 273.15}; }
    static Unit Newton() { return {1.0, {1,1,-2,0,0}}; } // kg*m/s^2
    static Unit Joule()  { return {1.0, {1,2,-2,0,0}}; } // kg*m^2/s^2
    static Unit Pascal() { return {1.0, {1,-1,-2,0,0}}; } // kg/(m*s^2)

    /**
     * @brief Simple string to Unit parser.
     */
    static Unit from_string(const std::string& s) {
        if (s == "m") return Meter();
        if (s == "s") return Second();
        if (s == "kg") return Kilogram();
        if (s == "K") return Kelvin();
        if (s == "C") return Celsius();
        if (s == "N") return Newton();
        if (s == "J") return Joule();
        if (s == "W") return {1.0, {1, 2, -3, 0, 0}};
        if (s == "kW") return {1000.0, {1, 2, -3, 0, 0}};
        if (s == "Pa") return Pascal();
        if (s == "m/s") return Meter() / Second();
        if (s == "m/s^2") return Meter() / Second().pow(2);
        if (s == "kg/m^3") return Kilogram() / (Meter().pow(3));
        return Dimensionless();
    }
};

} // namespace cones

#endif
