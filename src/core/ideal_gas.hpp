#ifndef CONES_CORE_IDEAL_GAS_HPP
#define CONES_CORE_IDEAL_GAS_HPP

#include "substance.hpp"
#include <map>

namespace cones {

// Ideal gas substance (name, R_gas [J/kg*K], C_p [J/kg*K])
class IdealGasSubstance : public Substance {
    std::string name_;
    double R_gas_; // J/(kg*K)
    double Cp_;    // J/(kg*K) for simplified model

public:
    IdealGasSubstance(std::string name, double R, double Cp) 
        : name_(std::move(name)), R_gas_(R), Cp_(Cp) {}

    std::string name() const override { return name_; }

    DualNumber evaluate(PropertyType target, const std::vector<PropertyArg>& inputs) const override {
        // Simple Ideal Gas Model
        // 1. PV = RT (rho = P / RT, v = RT / P)
        // 2. h = Cp * T
        // 3. u = Cv * T = (Cp - R) * T

        DualNumber T(0,0), P(0,0), rho(0,0), h(0,0), u(0,0), v(0,0);
        bool has_T=false, has_P=false, has_rho=false, has_h=false, has_u=false, has_v=false;

        for (const auto& in : inputs) {
            if (in.type == PropertyType::TEMPERATURE) { T = in.value; has_T = true; }
            if (in.type == PropertyType::PRESSURE) { P = in.value; has_P = true; }
            if (in.type == PropertyType::DENSITY) { rho = in.value; has_rho = true; }
            if (in.type == PropertyType::ENTHALPY) { h = in.value; has_h = true; }
            if (in.type == PropertyType::INTERNAL_ENERGY) { u = in.value; has_u = true; }
            if (in.type == PropertyType::SPECIFIC_VOLUME) { v = in.value; has_v = true; }
        }

        // Basic derivations to get T and P if possible
        if (!has_T) {
            if (has_h) { T = h / Cp_; has_T = true; }
            else if (has_u) { T = u / (Cp_ - R_gas_); has_T = true; }
            else if (has_P && has_rho) { T = P / (rho * R_gas_); has_T = true; }
            else if (has_P && has_v) { T = (P * v) / R_gas_; has_T = true; }
        }

        if (!has_P) {
            if (has_T && has_rho) { P = rho * R_gas_ * T; has_P = true; }
            else if (has_T && has_v) { P = (R_gas_ * T) / v; has_P = true; }
        }

        // --- Logic to solve for Target ---

        if (target == PropertyType::PRESSURE) {
            if (has_P) return P;
        }

        if (target == PropertyType::TEMPERATURE) {
            if (has_T) return T;
        }

        if (target == PropertyType::ENTHALPY) {
            if (has_T) return T * Cp_;
        }

        if (target == PropertyType::INTERNAL_ENERGY) {
            if (has_T) return T * (Cp_ - R_gas_);
        }

        if (target == PropertyType::DENSITY) {
            if (has_P && has_T) return P / (R_gas_ * T);
            if (has_v) return 1.0 / v;
        }

        if (target == PropertyType::SPECIFIC_VOLUME) {
            if (has_P && has_T) return (R_gas_ * T) / P;
            if (has_rho) return 1.0 / rho;
        }

        throw std::runtime_error("IdealGas (" + name_ + "): Insufficient properties for " + std::to_string((int)target));
    }
};

} // namespace cones

#endif
