#ifndef CONES_CORE_SYSTEM_HPP
#define CONES_CORE_SYSTEM_HPP

#include "expression.hpp"
#include "variable_registry.hpp"
#include "function_registry.hpp"
#include "substance_manager.hpp"
#include "constant_registry.hpp"
#include <Eigen/Dense>
#include <vector>
#include <memory>

namespace cones
{
    class System
    {
        std::vector<NodePtr> equations_;
        VariableRegistry registry_;
        FunctionRegistry function_registry_;
        SubstanceManager substance_manager_;
        ConstantRegistry constant_registry_;

    public:
        void add_equation(NodePtr eq) { equations_.push_back(std::move(eq)); }
        VariableRegistry &registry() { return registry_; }
        const VariableRegistry &registry() const { return registry_; }
        FunctionRegistry &function_registry() { return function_registry_; }
        SubstanceManager &substance_manager() { return substance_manager_; }
        ConstantRegistry &constant_registry() { return constant_registry_; }

        void evaluate(Eigen::VectorXd &f, Eigen::MatrixXd &j) const
        {
            const int n = static_cast<int>(equations_.size());
            auto active_indices = registry_.get_active_indices();
            const int m_active = static_cast<int>(active_indices.size());

            f.resize(n);
            j.resize(n, m_active);

            std::vector<DualNumber> dual_vals;
            dual_vals.reserve(registry_.size());
            for (size_t i = 0; i < registry_.size(); ++i)
                dual_vals.emplace_back(registry_.get_variable(i).value, 0.0);

            for (int i = 0; i < n; ++i)
            {
                for (int active_j = 0; active_j < m_active; ++active_j)
                {
                    int global_idx = active_indices[active_j];
                    dual_vals[global_idx].der = 1.0;

                    DualNumber res = equations_[i]->evaluate(dual_vals, registry_);

                    if (active_j == 0) f(i) = res.val;
                    j(i, active_j) = res.der;
                    dual_vals[global_idx].der = 0.0;
                }
            }
        }
    };
} // namespace cones

#endif
