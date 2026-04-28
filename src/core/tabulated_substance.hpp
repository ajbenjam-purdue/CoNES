#ifndef CONES_CORE_TABULATED_SUBSTANCE_HPP
#define CONES_CORE_TABULATED_SUBSTANCE_HPP

#include "substance.hpp"
#include "property_types.hpp"
#include <fstream>
#include <algorithm>
#include <vector>
#include <map>

namespace cones
{

    struct PropertyTable
    {
        std::vector<double> p_grid;
        std::vector<double> t_grid;
        std::vector<double> data;
    };

    /**
     * @brief Handles thermophysical properties using gridded binary tables.
     */
    class TabulatedSubstance : public Substance
    {
        std::string name_;
        std::map<PropertyType, PropertyTable> tables_;

    public:
        TabulatedSubstance(std::string name) : name_(std::move(name)) {}

        std::string name() const override { return name_; }

        void load_table(PropertyType prop, const std::string &path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open())
                throw std::runtime_error("Could not load table: " + path);

            char magic[4];
            file.read(magic, 4);

            int np, nt;
            file.read(reinterpret_cast<char *>(&np), sizeof(int));
            file.read(reinterpret_cast<char *>(&nt), sizeof(int));

            auto &table = tables_[prop];
            table.p_grid.resize(np);
            file.read(reinterpret_cast<char *>(table.p_grid.data()), np * sizeof(double));
            table.t_grid.resize(nt);
            file.read(reinterpret_cast<char *>(table.t_grid.data()), nt * sizeof(double));
            table.data.resize(np * nt);
            file.read(reinterpret_cast<char *>(table.data.data()), np * nt * sizeof(double));
        }

        DualNumber evaluate(PropertyType target, const std::vector<PropertyArg> &inputs) const override
        {
            for (const auto &in : inputs)
                if (in.type == target)
                    return in.value;

            DualNumber P(0, 0), T(0, 0), H(0, 0), S(0, 0), X(0, 0);
            bool has_P = false, has_T = false, has_H = false, has_S = false, has_X = false;

            for (const auto &in : inputs)
            {
                if (in.type == PropertyType::PRESSURE)
                {
                    P = in.value;
                    has_P = true;
                }
                if (in.type == PropertyType::TEMPERATURE)
                {
                    T = in.value;
                    has_T = true;
                }
                if (in.type == PropertyType::ENTHALPY)
                {
                    H = in.value;
                    has_H = true;
                }
                if (in.type == PropertyType::ENTROPY)
                {
                    S = in.value;
                    has_S = true;
                }
                if (in.type == PropertyType::QUALITY)
                {
                    X = in.value;
                    has_X = true;
                }
            }

            // --- 1. Saturation / Two-Phase Logic ---
            if (has_X && (has_P || has_T))
            {
                if (target == PropertyType::PRESSURE)
                    return evaluate(PropertyType::SATURATION_PRESSURE, {{PropertyType::TEMPERATURE, T}});
                if (target == PropertyType::TEMPERATURE)
                    return evaluate(PropertyType::SATURATION_TEMPERATURE, {{PropertyType::PRESSURE, P}});

                // h = hf + x*(hg - hf)
                if (target == PropertyType::ENTHALPY)
                {
                    DualNumber hf = evaluate(PropertyType::H_F, {{PropertyType::PRESSURE, P}, {PropertyType::TEMPERATURE, T}});
                    DualNumber hg = evaluate(PropertyType::H_G, {{PropertyType::PRESSURE, P}, {PropertyType::TEMPERATURE, T}});
                    return hf + X * (hg - hf);
                }
                if (target == PropertyType::ENTROPY)
                {
                    DualNumber sf = evaluate(PropertyType::S_F, {{PropertyType::PRESSURE, P}, {PropertyType::TEMPERATURE, T}});
                    DualNumber sg = evaluate(PropertyType::S_G, {{PropertyType::PRESSURE, P}, {PropertyType::TEMPERATURE, T}});
                    return sf + X * (sg - sf);
                }
            }

            // --- 2. Inverted Lookups (finding T from P,h or P,s) ---
            if (!has_T)
            {
                if (has_P && has_H)
                    T = evaluate_direct(PropertyType::T_PH, P, H);
                else if (has_P && has_S)
                    T = evaluate_direct(PropertyType::T_PS, P, S);
                else if (has_P && target == PropertyType::SATURATION_TEMPERATURE)
                    T = evaluate_direct(PropertyType::SATURATION_TEMPERATURE, P, {0, 0});

                if (T.val != 0.0)
                {
                    has_T = true;
                    if (target == PropertyType::TEMPERATURE)
                        return T;
                }
            }

            // --- 3. Final Direct Lookup ---
            auto it = tables_.find(target);
            if (it != tables_.end())
            {
                const auto &table = it->second;
                if (table.p_grid.size() == 1 && has_T)
                    return interpolate_1d_t(table, T);
                if (table.t_grid.size() == 1 && has_P)
                    return interpolate_1d_p(table, P);
                if (has_P && has_T)
                    return interpolate_2d(table, P, T);
            }

            // throw std::runtime_error("TabulatedSubstance (" + name_ + "): Insufficient inputs for " + property_to_string(target) + "()"); 
            return {1e9, 0.0};
        }

    private:
        DualNumber evaluate_direct(PropertyType type, DualNumber p1, DualNumber p2) const
        {
            auto it = tables_.find(type);
            if (it == tables_.end())
                throw std::runtime_error("Table not found: " + property_to_string(type));

            const auto &table = it->second;
            if (table.p_grid.size() == 1)
                return interpolate_1d_t(table, p2);
            if (table.t_grid.size() == 1)
                return interpolate_1d_p(table, p1);
            return interpolate_2d(table, p1, p2);
        }

        DualNumber interpolate_1d_t(const PropertyTable &table, DualNumber t_dual) const
        {
            double t = t_dual.val;
            int nt = (int)table.t_grid.size();
            int it = 0;
            
            auto it_t = std::lower_bound(table.t_grid.begin(), table.t_grid.end(), t);
            it = std::distance(table.t_grid.begin(), it_t) - 1;
            
            if (it < 0) it = 0;
            if (it >= nt - 1) it = nt - 2;

            double t0 = table.t_grid[it], t1 = table.t_grid[it + 1];
            double q0 = table.data[it], q1 = table.data[it + 1];
            double dt = t1 - t0;
            
            // Linear extrapolation/interpolation
            double val = q0 + (t - t0) * (q1 - q0) / dt;
            double dq_dt = (q1 - q0) / dt;
            
            return {val, dq_dt * t_dual.der};
        }

        DualNumber interpolate_1d_p(const PropertyTable &table, DualNumber p_dual) const
        {
            double p = p_dual.val;
            int np = (int)table.p_grid.size();
            int ip = 0;

            auto it_p = std::lower_bound(table.p_grid.begin(), table.p_grid.end(), p);
            ip = std::distance(table.p_grid.begin(), it_p) - 1;

            if (ip < 0) ip = 0;
            if (ip >= np - 1) ip = np - 2;

            double p0 = table.p_grid[ip], p1 = table.p_grid[ip + 1];
            double q0 = table.data[ip], q1 = table.data[ip + 1];
            double dp = p1 - p0;
            
            double val = q0 + (p - p0) * (q1 - q0) / dp;
            double dq_dp = (q1 - q0) / dp;

            return {val, dq_dp * p_dual.der};
        }

        DualNumber interpolate_2d(const PropertyTable &table, DualNumber p_dual, DualNumber t_dual) const
        {
            double p = p_dual.val, t = t_dual.val;
            int np = (int)table.p_grid.size();
            int nt = (int)table.t_grid.size();

            auto it_p = std::lower_bound(table.p_grid.begin(), table.p_grid.end(), p);
            auto it_t = std::lower_bound(table.t_grid.begin(), table.t_grid.end(), t);
            
            int ip = std::distance(table.p_grid.begin(), it_p) - 1;
            int it = std::distance(table.t_grid.begin(), it_t) - 1;

            if (ip < 0) ip = 0;
            if (ip >= np - 1) ip = np - 2;
            if (it < 0) it = 0;
            if (it >= nt - 1) it = nt - 2;

            double p0 = table.p_grid[ip], p1 = table.p_grid[ip + 1], t0 = table.t_grid[it], t1 = table.t_grid[it + 1];
            double q00 = table.data[ip * nt + it];
            double q01 = table.data[ip * nt + (it + 1)];
            double q10 = table.data[(ip + 1) * nt + it];
            double q11 = table.data[(ip + 1) * nt + (it + 1)];

            double dp = p1 - p0, dt = t1 - t0, u = (p - p0) / dp, v = (t - t0) / dt;
            
            // Bilinear interpolation/extrapolation
            double val = (1 - u) * (1 - v) * q00 + u * (1 - v) * q10 + (1 - u) * v * q01 + u * v * q11;
            double df_dp = (-(1 - v) * q00 + (1 - v) * q10 - v * q01 + v * q11) / dp;
            double df_dt = (-(1 - u) * q00 - u * q10 + (1 - u) * q01 + u * q11) / dt;
            
            return {val, df_dp * p_dual.der + df_dt * t_dual.der};
        }
    };

} // namespace cones

#endif
