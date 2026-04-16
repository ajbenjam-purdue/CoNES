#ifndef CONES_CORE_IDEAL_GAS_HPP
#define CONES_CORE_IDEAL_GAS_HPP

#include "substance.hpp"
#include <map>

namespace cones {

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
        // 1. PV = RT (rho = P / RT)
        // 2. h = Cp * T

        // We need T and P or T and rho or h and P etc.
        DualNumber T(0,0), P(0,0), rho(0,0), h(0,0);
        bool has_T=false, has_P=false, has_rho=false, has_h=false;

        for (const auto& in : inputs) {
            if (in.type == PropertyType::TEMPERATURE) { T = in.value; has_T = true; }
            if (in.type == PropertyType::PRESSURE) { P = in.value; has_P = true; }
            if (in.type == PropertyType::DENSITY) { rho = in.value; has_rho = true; }
            if (in.type == PropertyType::ENTHALPY) { h = in.value; has_h = true; }
        }

        // --- Logic to solve for Target ---

        if (target == PropertyType::PRESSURE) {
            if (has_T && has_rho) return rho * R_gas_ * T;
            if (has_h && has_rho) return rho * R_gas_ * (h / Cp_);
        }

        if (target == PropertyType::TEMPERATURE) {
            if (has_h) return h / Cp_;
            if (has_P && has_rho) return P / (rho * R_gas_);
        }

        if (target == PropertyType::ENTHALPY) {
            if (has_T) return T * Cp_;
            if (has_P && has_rho) return (P / (rho * R_gas_)) * Cp_;
        }

        throw std::runtime_error("IdealGas (" + name_ + "): Insufficient properties for " + std::to_string((int)target));
    }
};

} // namespace cones

#endif
