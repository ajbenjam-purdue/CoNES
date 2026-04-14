#ifndef CONES_CORE_VARIABLE_REGISTRY_HPP
#define CONES_CORE_VARIABLE_REGISTRY_HPP

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
        std::string unit = "";
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
            variables_.push_back({name, index});
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
        void set_unit(int index, const std::string &unit) { variables_.at(index).unit = unit; }

        // Getters
        const Variable &get_variable(int index) const { return variables_.at(index); }
        size_t size() const { return variables_.size(); }

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
