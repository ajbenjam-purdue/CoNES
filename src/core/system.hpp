#ifndef CONES_CORE_SYSTEM_HPP
#define CONES_CORE_SYSTEM_HPP

#include "expression.hpp"
#include "variable_registry.hpp"
#include "function_registry.hpp"
#include "substance_manager.hpp"
#include "constant_registry.hpp"
#include "unit_registry.hpp"
#include "../lang/definition_registry.hpp"
#include <Eigen/Dense>
#include <vector>
#include <memory>

#include <unordered_set>

namespace cones
{
    class System
    {
        std::vector<NodePtr> equations_;
        std::vector<int> equation_lines_;
        std::unordered_set<std::string> equation_strings_;
        VariableRegistry registry_;
        FunctionRegistry function_registry_;
        SubstanceManager substance_manager_;
        ConstantRegistry constant_registry_;
        UnitRegistry unit_registry_;
        DefinitionRegistry definition_registry_;

    public:
        void add_equation(NodePtr eq, int line = -1) { 
            std::string s = eq->to_string();
            if (equation_strings_.count(s)) return;
            equation_strings_.insert(s);
            equations_.push_back(std::move(eq)); 
            equation_lines_.push_back(line);
        }
        size_t get_equation_count() const { return equations_.size(); }
        const std::vector<NodePtr>& get_equations() const { return equations_; }
        std::string get_equation_plaintext(size_t index) const {
            if (index >= equations_.size()) return "";
            return equations_[index]->to_string();
        }
        int get_equation_line(size_t index) const {
            if (index >= equation_lines_.size()) return -1;
            return equation_lines_[index];
        }
        VariableRegistry &registry() { return registry_; } // Yield the address for the System's VariableRegistry
        const VariableRegistry &registry() const { return registry_; }
        FunctionRegistry &function_registry() { return function_registry_; }
        SubstanceManager &substance_manager() { return substance_manager_; }
        ConstantRegistry &constant_registry() { return constant_registry_; }
        UnitRegistry &unit_registry() { return unit_registry_; }
        const UnitRegistry &unit_registry() const { return unit_registry_; }
        DefinitionRegistry &definition_registry() { return definition_registry_; }
        const DefinitionRegistry &definition_registry() const { return definition_registry_; }

        void evaluate(Eigen::VectorXd &f, Eigen::MatrixXd &j) const
        {
            const int n = static_cast<int>(equations_.size());
            auto active_indices = registry_.get_active_indices();
            const int m_active = static_cast<int>(active_indices.size());

            f.resize(n);
            j.resize(n, m_active);

            std::vector<DualRow> dual_rows;
            dual_rows.reserve(registry_.size());
            for (size_t i = 0; i < registry_.size(); ++i)
                dual_rows.emplace_back(registry_.get_variable(i).value, m_active);

            for (int active_j = 0; active_j < m_active; ++active_j)
            {
                int global_idx = active_indices[active_j];
                dual_rows[global_idx].der(active_j) = 1.0;
            }

            for (int i = 0; i < n; ++i)
            {
                try {
                    DualRow res = equations_[i]->evaluate_row(dual_rows, registry_);
                    
                    if (std::isnan(res.val)) f(i) = 1e9;
                    else f(i) = res.val;

                    for (int active_j = 0; active_j < m_active; ++active_j)
                    {
                        if (std::isnan(res.der(active_j))) j(i, active_j) = 0.0;
                        else j(i, active_j) = res.der(active_j);
                    }
                } catch (...) {
                    f(i) = 1e9;
                    j.row(i).setZero();
                }
            }
        }

        void evaluate_subset(const std::vector<int> &eq_indices, Eigen::VectorXd &f, Eigen::MatrixXd &j) const
        {
            const int n = static_cast<int>(eq_indices.size());
            auto active_indices = registry_.get_active_indices();
            const int m_active = static_cast<int>(active_indices.size());

            f.resize(n);
            j.resize(n, m_active);

            std::vector<DualRow> dual_rows;
            dual_rows.reserve(registry_.size());
            for (size_t i = 0; i < registry_.size(); ++i)
                dual_rows.emplace_back(registry_.get_variable(i).value, m_active);

            for (int active_j = 0; active_j < m_active; ++active_j)
            {
                int global_idx = active_indices[active_j];
                dual_rows[global_idx].der(active_j) = 1.0;
            }

            for (int i = 0; i < n; ++i)
            {
                int eq_idx = eq_indices[i];
                try {
                    DualRow res = equations_[eq_idx]->evaluate_row(dual_rows, registry_);
                    
                    if (std::isnan(res.val)) f(i) = 1e9;
                    else f(i) = res.val;

                    for (int active_j = 0; active_j < m_active; ++active_j)
                    {
                        if (std::isnan(res.der(active_j))) j(i, active_j) = 0.0;
                        else j(i, active_j) = res.der(active_j);
                    }
                } catch (...) {
                    f(i) = 1e9;
                    j.row(i).setZero();
                }
            }
        }
    };
} // namespace cones

#endif
