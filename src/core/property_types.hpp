#ifndef CONES_CORE_PROPERTY_TYPES_HPP
#define CONES_CORE_PROPERTY_TYPES_HPP

#include <string>

namespace cones
{

    enum class PropertyType
    {
        TEMPERATURE, // T
        PRESSURE,    // P
        ENTHALPY,    // h
        ENTROPY,     // s
        QUALITY,     // x
        DENSITY,     // rho
        UNKNOWN
    };

    /*
     * @brief Returns the enum PropertyType according to the input string:
     * @param &s String to process
     * @return PropertyType
     * 
     * - "T": PropertyType::TEMPERATURE
     * 
     * - "P": PropertyType::PRESSURE
     * 
     * - "h": PropertyType::ENTHALPY
     * 
     * - "s": PropertyType::ENTROPY
     * 
     * - "x": PropertyType::QUALITY
     * 
     * - "rho" || "r": PropertyType::DENSITY
    */
    inline PropertyType string_to_property(const std::string &s)
    {
        if (s == "T")
            return PropertyType::TEMPERATURE;
        if (s == "P")
            return PropertyType::PRESSURE;
        if (s == "h")
            return PropertyType::ENTHALPY;
        if (s == "s")
            return PropertyType::ENTROPY;
        if (s == "x")
            return PropertyType::QUALITY;
        if (s == "rho" || s == "r")
            return PropertyType::DENSITY;
        return PropertyType::UNKNOWN;
    }

} // namespace cones

#endif
