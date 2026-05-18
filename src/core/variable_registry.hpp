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
    };

    class VariableRegistry
    {
        std::unordered_map<std::string, int> name_to_index_;
        std::vector<Variable> variables_;

    public:
        int register_variable(const std::string &name)
        {
            auto it = name_to_index_.find(name);
            if (it != name_to_index_.end())
                return it->second;

            int index = static_cast<int>(variables_.size());
            name_to_index_[name] = index;
            Variable v;
            v.name = name;
            v.index = index;
            variables_.push_back(v);
            return index;
        }

        // Setters
        void set_value(int index, double val) { variables_.at(index).value = val; }
        void set_bounds(int index, double lower, double upper)
        {
            variables_.at(index).lower_bound = lower;
            variables_.at(index).upper_bound = upper;
        }
        void set_fixed(int index, bool fixed) { variables_.at(index).is_fixed = fixed; }
        void set_reserved(int index, bool reserved) { variables_.at(index).is_reserved = reserved; }
        void set_unit(int index, const Unit &unit, const std::string &name = "")
        {
            variables_.at(index).unit = unit;
            if (!name.empty())
                variables_.at(index).unit_name = name;
        }
        void set_unit(int index, const std::string &name)
        {
            variables_.at(index).unit = Unit::from_string(name);
            variables_.at(index).unit_name = name;
        }

        /**
         * @brief Suggests a ballpark guess based on units if the value is still at its default (1.0).
         */
        void suggest_guess(int index, const Unit &u)
        {
            auto &v = variables_.at(index);
            if (v.value != 1.0 || v.is_fixed)
                return;

            // Pressure (Pa)
            if (u.dims == std::vector<int>{1, -1, -2, 0, 0})
                v.value = 101325.0;
            // Temperature (K)
            else if (u.dims == std::vector<int>{0, 0, 0, 1, 0})
                v.value = 293.15;
            // Enthalpy / Specific Energy (J/kg)
            else if (u.dims == std::vector<int>{0, 2, -2, 0, 0})
                v.value = 250000.0;
            // Density (kg/m^3)
            else if (u.dims == std::vector<int>{1, -3, 0, 0, 0})
                v.value = 1.2;
            // Energy (J)
            else if (u.dims == std::vector<int>{1, 2, -2, 0, 0})
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
