#ifndef CONES_CORE_SYSTEM_HPP
#define CONES_CORE_SYSTEM_HPP

#include "expression.hpp"
#include "variable_registry.hpp"
#include <Eigen/Dense>
#include <vector>
#include <memory>

namespace cones
{

    /**
     * @brief Represents a system of coupled nonlinear equations: F(x) = 0.
     *
     * This class manages the equations (as ASTs) and the variable registry.
     * It provides the interface for the numerical solver to obtain the
     * residual vector and the Jacobian matrix.
     */
    class System
    {
        std::vector<NodePtr> equations_;
        VariableRegistry registry_;

    public:
        /**
         * @brief Adds a new equation to the system.
         * Expects an expression that evaluates to 0 at the solution.
         */
        void add_equation(NodePtr eq)
        {
            equations_.push_back(std::move(eq));
        }

        /**
         * @brief Returns the variable registry for adding/mapping variable names.
         */
        VariableRegistry &registry() { return registry_; }
        const VariableRegistry &registry() const { return registry_; }

        /**
         * @brief Number of equations in the system.
         */
        size_t num_equations() const { return equations_.size(); }

        /**
         * @brief Number of registered variables.
         */
        size_t num_variables() const { return registry_.size(); }

        /**
         * @brief Evaluates the system residuals F(x) and the Jacobian J(x).
         *
         * @param x Current guess for the variables.
         * @param f Output: Residual vector F(x).
         * @param j Output: Jacobian matrix J(x) where J_ij = dF_i / dx_j.
         */
        void evaluate(const Eigen::VectorXd &x, Eigen::VectorXd &f, Eigen::MatrixXd &j) const
        {
            const int n = static_cast<int>(equations_.size());
            const int m = static_cast<int>(registry_.size());

            f.resize(n);
            j.resize(n, m);

            // For each equation Fi
            for (int i = 0; i < n; ++i)
            {
                // For each variable xj, we calculate dFi/dxj
                for (int j_idx = 0; j_idx < m; ++j_idx)
                {
                    // Prepare input vector with Dual Numbers
                    // We seed the derivative for variable j_idx
                    std::vector<DualNumber> dual_x;
                    dual_x.reserve(m);
                    for (int k = 0; k < m; ++k)
                    {
                        dual_x.emplace_back(x(k), (k == j_idx ? 1.0 : 0.0));
                    }

                    // Evaluate AST
                    DualNumber res = equations_[i]->evaluate(dual_x);

                    // On the first variable pass, store the function value
                    if (j_idx == 0)
                    {
                        f(i) = res.val;
                    }

                    // Store the derivative in the Jacobian
                    j(i, j_idx) = res.der;
                }
            }
        }
    };

} // namespace cones

#endif // CONES_CORE_SYSTEM_HPP
