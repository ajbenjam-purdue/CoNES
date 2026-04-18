#ifndef CONES_CORE_TABULATED_SUBSTANCE_HPP
#define CONES_CORE_TABULATED_SUBSTANCE_HPP

#include "substance.hpp"
#include <fstream>
#include <algorithm>
#include <vector>

namespace cones {

/**
 * @brief Handles thermophysical properties using gridded binary tables.
 */
class TabulatedSubstance : public Substance {
    std::string name_;
    std::vector<double> p_grid_;
    std::vector<double> t_grid_;
    std::vector<double> h_table_; // Enthalpy table [p][t]

public:
    TabulatedSubstance(std::string name) : name_(std::move(name)) {}

    std::string name() const override { return name_; }

    /**
     * @brief Loads a binary property table.
     */
    void load_table(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) throw std::runtime_error("Could not load table: " + path);

        char magic[4];
        file.read(magic, 4); // "CNES"

        int np, nt;
        file.read(reinterpret_cast<char*>(&np), sizeof(int));
        file.read(reinterpret_cast<char*>(&nt), sizeof(int));

        p_grid_.resize(np);
        t_grid_.resize(nt);
        h_table_.resize(np * nt);

        file.read(reinterpret_cast<char*>(p_grid_.data()), np * sizeof(double));
        file.read(reinterpret_cast<char*>(t_grid_.data()), nt * sizeof(double));
        file.read(reinterpret_cast<char*>(h_table_.data()), np * nt * sizeof(double));
    }

    DualNumber evaluate(PropertyType target, const std::vector<PropertyArg>& inputs) const override {
        if (target != PropertyType::ENTHALPY) 
            throw std::runtime_error("TabulatedSubstance currently only supports Enthalpy lookups.");

        DualNumber P(0,0), T(0,0);
        for (const auto& in : inputs) {
            if (in.type == PropertyType::PRESSURE) P = in.value;
            if (in.type == PropertyType::TEMPERATURE) T = in.value;
        }

        return interpolate_2d(P, T);
    }

private:
    DualNumber interpolate_2d(DualNumber p_dual, DualNumber t_dual) const {
        double p = p_dual.val;
        double t = t_dual.val;

        // Find indices in grids
        auto it_p = std::lower_bound(p_grid_.begin(), p_grid_.end(), p);
        auto it_t = std::lower_bound(t_grid_.begin(), t_grid_.end(), t);

        int ip = std::distance(p_grid_.begin(), it_p) - 1;
        int it = std::distance(t_grid_.begin(), it_t) - 1;

        // Bound checking
        if (ip < 0 || ip >= (int)p_grid_.size() - 1 || it < 0 || it >= (int)t_grid_.size() - 1) {
            // Return edge value with a large slope to push solver back
            return { h_table_[0], 0.0 }; 
        }

        // Get the 4 corners of the cell
        double p0 = p_grid_[ip], p1 = p_grid_[ip+1];
        double t0 = t_grid_[it], t1 = t_grid_[it+1];

        double q00 = h_table_[ip * t_grid_.size() + it];
        double q01 = h_table_[ip * t_grid_.size() + (it+1)];
        double q10 = h_table_[(ip+1) * t_grid_.size() + it];
        double q11 = h_table_[(ip+1) * t_grid_.size() + (it+1)];

        // Bilinear Interpolation
        double dp = p1 - p0;
        double dt = t1 - t0;
        double u = (p - p0) / dp;
        double v = (t - t0) / dt;

        double val = (1-u)*(1-v)*q00 + u*(1-v)*q10 + (1-u)*v*q01 + u*v*q11;

        // Calculate Slopes for Automatic Differentiation
        // df/dp
        double df_dp = (-(1-v)*q00 + (1-v)*q10 - v*q01 + v*q11) / dp;
        // df/dt
        double df_dt = (-(1-u)*q00 - u*q10 + (1-u)*q01 + u*q11) / dt;

        // Chain Rule: f' = (df/dp)*p' + (df/dt)*t'
        return { val, df_dp * p_dual.der + df_dt * t_dual.der };
    }
};

} // namespace cones

#endif
