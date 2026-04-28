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
    std::string summary() const override { 
        return "Ideal Gas Model [R=" + std::to_string((int)R_gas_) + " J/kg*K, Cp=" + std::to_string((int)Cp_) + " J/kg*K]"; 
    }

    DualNumber evaluate(PropertyType target, const std::vector<PropertyArg>& inputs) const override {
        DualNumber T(0,0), P(0,0), rho(0,0), h(0,0), u(0,0), v(0,0), s(0,0);
        bool has_T=false, has_P=false, has_rho=false, has_h=false, has_u=false, has_v=false, has_s=false;

        const double T_ref = 298.15;
        const double P_ref = 101325.0;

        for (const auto& in : inputs) {
            if (in.type == PropertyType::TEMPERATURE) { T = in.value; has_T = true; }
            if (in.type == PropertyType::PRESSURE) { P = in.value; has_P = true; }
            if (in.type == PropertyType::DENSITY) { rho = in.value; has_rho = true; }
            if (in.type == PropertyType::ENTHALPY) { h = in.value; has_h = true; }
            if (in.type == PropertyType::INTERNAL_ENERGY) { u = in.value; has_u = true; }
            if (in.type == PropertyType::SPECIFIC_VOLUME) { v = in.value; has_v = true; }
            if (in.type == PropertyType::ENTROPY) { s = in.value; has_s = true; }
        }

        // Basic derivations to get T and P if possible
        if (!has_T) {
            if (has_h) { T = h / Cp_; has_T = true; }
            else if (has_u) { T = u / (Cp_ - R_gas_); has_T = true; }
            else if (has_s && has_P) {
                // s = Cp*ln(T/T_ref) - R*ln(P/P_ref)
                // ln(T/T_ref) = (s + R*ln(P/P_ref)) / Cp
                T = T_ref * exp((s + R_gas_ * log(P / P_ref)) / Cp_);
                has_T = true;
            }
            else if (has_P && has_rho) { T = P / (rho * R_gas_); has_T = true; }
            else if (has_P && has_v) { T = (P * v) / R_gas_; has_T = true; }
        }

        if (!has_P) {
            if (has_T && has_rho) { P = rho * R_gas_ * T; has_P = true; }
            else if (has_T && has_v) { P = (R_gas_ * T) / v; has_P = true; }
            else if (has_T && has_s) {
                // s = Cp*ln(T/T_ref) - R*ln(P/P_ref)
                // R*ln(P/P_ref) = Cp*ln(T/T_ref) - s
                // ln(P/P_ref) = (Cp*ln(T/T_ref) - s) / R
                P = P_ref * exp((Cp_ * log(T / T_ref) - s) / R_gas_);
                has_P = true;
            }
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

        if (target == PropertyType::ENTROPY) {
            if (has_T && has_P) {
                return Cp_ * log(T / T_ref) - R_gas_ * log(P / P_ref);
            }
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

        // throw std::runtime_error("IdealGas (" + name_ + "): Insufficient properties for " + std::to_string((int)target));
        return {1e9, 0.0};
    }
};

} // namespace cones

#endif
