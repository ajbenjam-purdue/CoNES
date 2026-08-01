#ifndef CONES_CORE_VARIABLE_REGISTRY_HPP
#define CONES_CORE_VARIABLE_REGISTRY_HPP

#include "unit.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <limits>
#include <Eigen/Dense>

namespace cones
{

    struct Variable
    {
        std::string name;
        int index;
        double value = 1.0; // Current value (or initial guess)
        double lower_bound = -std::numeric_limits<double>::infinity();
        double upper_bound = std::numeric_limits<double>::infinity();
        bool is_fixed = false;
        bool is_reserved = false; // Materials/Reserved keywords
        Unit unit = Unit::Dimensionless();
        std::string unit_name = "";
        int line = -1;
    };

    class VariableRegistry
    {
        std::unordered_map<std::string, int> name_to_index_;
        std::vector<Variable> variables_;

    public:
        int register_variable(const std::string &name, int line = -1)
        {
            auto it = name_to_index_.find(name);
            if (it != name_to_index_.end()) {
                if (variables_[it->second].line == -1) variables_[it->second].line = line;
                return it->second;
            }

            int index = static_cast<int>(variables_.size());
            name_to_index_[name] = index;
            Variable v;
            v.name = name;
            v.index = index;
            v.line = line;
            variables_.push_back(v);
            return index;
        }

        // Setters
        void set_value(int index, double val, bool is_si = false)
        {
            auto &v = variables_.at(index);
            if (!is_si && !v.unit.is_dimensionless())
                v.value = (val + v.unit.offset) * v.unit.scale;
            else
                v.value = val;
        }

        void set_lower_bound(int index, double val, bool is_si = false)
        {
            auto &v = variables_.at(index);
            if (!is_si && !v.unit.is_dimensionless())
                v.lower_bound = (val + v.unit.offset) * v.unit.scale;
            else
                v.lower_bound = val;
        }

        void set_upper_bound(int index, double val, bool is_si = false)
        {
            auto &v = variables_.at(index);
            if (!is_si && !v.unit.is_dimensionless())
                v.upper_bound = (val + v.unit.offset) * v.unit.scale;
            else
                v.upper_bound = val;
        }

        void set_bounds(int index, double lower, double upper, bool is_si = false)
        {
            set_lower_bound(index, lower, is_si);
            set_upper_bound(index, upper, is_si);
        }

        void set_fixed(int index, bool fixed) { variables_.at(index).is_fixed = fixed; }
        void set_reserved(int index, bool reserved) { variables_.at(index).is_reserved = reserved; }
        void set_unit(int index, const Unit &unit, const std::string &name = "")
        {
            auto &v = variables_.at(index);
            // scale the value to SI prior to setting guess
            if (v.unit.is_dimensionless() && !unit.is_dimensionless())
            {
                v.value = (v.value + unit.offset) * unit.scale;
                if (v.lower_bound > -1e20)
                    v.lower_bound = (v.lower_bound + unit.offset) * unit.scale;
                if (v.upper_bound < 1e20)
                    v.upper_bound = (v.upper_bound + unit.offset) * unit.scale;
            }
            v.unit = unit;
            if (!name.empty())
                v.unit_name = name;
            
            // Apply sanity bounds for physical quantities
            if (unit.requires_positivity()) {
                if (v.lower_bound < 0) v.lower_bound = 1e-7;
            }
        }

        /**
         * @brief Suggests a ballpark guess based on units if the value is still at its default (1.0, or scaled 1.0).
         */
        void suggest_guess(int index, const Unit &u)
        {
            auto &v = variables_.at(index);
            double default_val = (1.0 + u.offset) * u.scale;
            if (v.is_fixed || (std::abs(v.value - 1.0) > 1e-9 && std::abs(v.value - default_val) > 1e-9))
                return;

            // Pressure (Pa)
            if (u.dims == std::vector<int>{1, -1, -2, 0, 0, 0})
                v.value = 101325.0;
            // Temperature (K)
            else if (u.dims == std::vector<int>{0, 0, 0, 1, 0, 0})
                v.value = 293.15;
            // Enthalpy / Specific Energy (J/kg)
            else if (u.dims == std::vector<int>{0, 2, -2, 0, 0, 0})
                v.value = 250000.0;
            // Entropy (J/kg*K)
            else if (u.dims == std::vector<int>{0, 2, -2, -1, 0, 0})
                v.value = 1500.0;
            // Density (kg/m^3)
            else if (u.dims == std::vector<int>{1, -3, 0, 0, 0, 0})
                v.value = 1.2;
            // Energy (J)
            else if (u.dims == std::vector<int>{1, 2, -2, 0, 0, 0})
                v.value = 1000.0;
        }

        // Getters
        size_t size() const { return variables_.size(); }

        int get_index(const std::string &name) const
        {
            auto it = name_to_index_.find(name);
            if (it == name_to_index_.end())
                return -1;
            return it->second;
        }

        // TODO: Correct undefined behavior for -1/EOB/missing name
        const Variable &get_variable(int index) const // Get the variable value at the provided index
        {
            return variables_.at(index);
        }
        const Variable &get_variable(const std::string &name) const // Get the variable value with the provided name
        {
            return variables_.at(get_index(name));
        }

        /**
         * @brief Returns the indices of variables that are NOT fixed, or the Degrees of Freedom (DOF) for the solver
         */
        std::vector<int> get_active_indices() const
        {
            std::vector<int> active;
            for (const auto &var : variables_)
            {
                if (!var.is_fixed)
                    active.push_back(var.index);
            }
            return active;
        }

        bool is_active(int index) const
        {
            if (index < 0 || index >= static_cast<int>(variables_.size())) return false;
            return !variables_[index].is_fixed;
        }

        /**
         * @brief Gets the values of only the active variables.
         */
        Eigen::VectorXd get_active_values() const
        {
            auto active = get_active_indices();
            Eigen::VectorXd x(active.size());
            for (size_t i = 0; i < active.size(); ++i)
            {
                x(i) = variables_[active[i]].value;
            }
            return x;
        }

        /**
         * @brief Updates the registry with new values for active variables.
         */
        void update_active_values(const Eigen::VectorXd &x_active)
        {
            auto active = get_active_indices();
            for (size_t i = 0; i < active.size(); ++i)
            {
                variables_[active[i]].value = x_active(i);
            }
        }

        void apply_bounds()
        {
            for (auto &var : variables_)
            {
                if (var.value < var.lower_bound)
                    var.value = var.lower_bound;
                if (var.value > var.upper_bound)
                    var.value = var.upper_bound;
            }
        }
    };

} // namespace cones

#endif
