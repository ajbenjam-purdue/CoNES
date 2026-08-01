#ifndef CONES_CORE_PROPERTY_TYPES_HPP
#define CONES_CORE_PROPERTY_TYPES_HPP

#include <string>

namespace cones
{

    enum class PropertyType
    {
        TEMPERATURE,      // T
        PRESSURE,         // P
        ENTHALPY,         // h
        ENTROPY,          // s
        INTERNAL_ENERGY,  // u
        SPECIFIC_VOLUME,  // v
        DENSITY,          // rho
        QUALITY,          // x
        VISCOSITY,        // mu
        CONDUCTIVITY,     // k
        PRANDTL,          // Pr
        REYNOLDS,         // Re
        SATURATION_PRESSURE,    // Psat
        SATURATION_TEMPERATURE, // Tsat
        T_SAT_F, T_SAT_G,       // Bubble and dew saturation temperatures (glide)
        H_F, H_G, S_F, S_G,     // Saturated liquid/vapor h and s
        T_PH, T_PS,             // Inverted T(P,h) and T(P,s)
        UNKNOWN
    };

    /**
     * @brief Returns the enum PropertyType according to the input string:
     * @param &s String to process
     * @return PropertyType
     */
    inline PropertyType string_to_property(const std::string &s)
    {
        if (s == "T" || s == "Temperature")
            return PropertyType::TEMPERATURE;
        if (s == "P" || s == "Pressure")
            return PropertyType::PRESSURE;
        if (s == "h" || s == "Enthalpy")
            return PropertyType::ENTHALPY;
        if (s == "s" || s == "Entropy")
            return PropertyType::ENTROPY;
        if (s == "u" || s == "InternalEnergy")
            return PropertyType::INTERNAL_ENERGY;
        if (s == "v" || s == "SpecificVolume")
            return PropertyType::SPECIFIC_VOLUME;
        if (s == "rho" || s == "r" || s == "Density")
            return PropertyType::DENSITY;
        if (s == "x" || s == "Quality")
            return PropertyType::QUALITY;
        if (s == "mu" || s == "Viscosity")
            return PropertyType::VISCOSITY;
        if (s == "k" || s == "Conductivity")
            return PropertyType::CONDUCTIVITY;
        if (s == "Pr" || s == "Prandtl")
            return PropertyType::PRANDTL;
        if (s == "Re" || s == "Reynolds")
            return PropertyType::REYNOLDS;
        if (s == "Psat")
            return PropertyType::SATURATION_PRESSURE;
        if (s == "Tsat")
            return PropertyType::SATURATION_TEMPERATURE;
        if (s == "Tsat_f" || s == "T_sat_f" || s == "Tf_sat") return PropertyType::T_SAT_F;
        if (s == "Tsat_g" || s == "T_sat_g" || s == "Tg_sat") return PropertyType::T_SAT_G;
        if (s == "hf") return PropertyType::H_F;
        if (s == "hg") return PropertyType::H_G;
        if (s == "sf") return PropertyType::S_F;
        if (s == "sg") return PropertyType::S_G;
        if (s == "T_ph") return PropertyType::T_PH;
        if (s == "T_ps") return PropertyType::T_PS;
        return PropertyType::UNKNOWN;
    }

    inline std::string property_to_string(PropertyType p)
    {
        switch (p) {
            case PropertyType::TEMPERATURE:     return "Temperature";
            case PropertyType::PRESSURE:        return "Pressure";
            case PropertyType::ENTHALPY:        return "Enthalpy";
            case PropertyType::ENTROPY:         return "Entropy";
            case PropertyType::INTERNAL_ENERGY: return "InternalEnergy";
            case PropertyType::SPECIFIC_VOLUME: return "SpecificVolume";
            case PropertyType::DENSITY:         return "Density";
            case PropertyType::QUALITY:         return "Quality";
            case PropertyType::VISCOSITY:       return "Viscosity";
            case PropertyType::CONDUCTIVITY:    return "Conductivity";
            case PropertyType::PRANDTL:         return "Prandtl";
            case PropertyType::REYNOLDS:        return "Reynolds";
            case PropertyType::SATURATION_PRESSURE:    return "Psat";
            case PropertyType::SATURATION_TEMPERATURE: return "Tsat";
            case PropertyType::T_SAT_F: return "Tsat_f";
            case PropertyType::T_SAT_G: return "Tsat_g";
            case PropertyType::H_F: return "hf";
            case PropertyType::H_G: return "hg";
            case PropertyType::S_F: return "sf";
            case PropertyType::S_G: return "sg";
            case PropertyType::T_PH: return "T_ph";
            case PropertyType::T_PS: return "T_ps";
            default:                            return "Unknown";
        }
    }

} // namespace cones

#endif
