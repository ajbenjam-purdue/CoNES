#ifndef CONES_SOLVER_NEWTON_SOLVER_HPP
#define CONES_SOLVER_NEWTON_SOLVER_HPP

#include "../core/system.hpp"
#include <Eigen/Dense>
#include <iostream>
#include <stdexcept>

namespace cones
{

    /**
     * @brief Implements the Newton-Raphson method for solving systems of nonlinear equations.
     *
     * Ref: https://www.geeksforgeeks.org/engineering-mathematics/newton-raphson-method/
     */
    class NewtonSolver
    {
        double tolerance_;
        int max_iterations_;
        bool verbose_;

    public:
        explicit NewtonSolver(double tol = 1e-8, int max_iter = 1000, bool verbose = false)
            : tolerance_(tol), max_iterations_(max_iter), verbose_(verbose) {}

        /**
         * @brief Solves the system F(x) = 0 starting from an initial guess.
         *
         * @param system The system of equations to solve.
         * @param initial_guess Starting point for the iteration.
         * @return Eigen::VectorXd The solution vector x.
         */
        Eigen::VectorXd solve(const System &system, const Eigen::VectorXd &initial_guess)
        {
            Eigen::VectorXd x = initial_guess;
            Eigen::VectorXd f;
            Eigen::MatrixXd j;

            if (verbose_)
            {
                std::cout << "Starting Newton-Raphson Solver..." << std::endl;
                std::cout << "Iter\tResidual Norm" << std::endl;
            }

            for (int iter = 0; iter < max_iterations_; ++iter)
            {
                // Evaluate Residuals and Jacobian
                system.evaluate(x, f, j);

                // Check for convergence (Infinity norm of residual)
                double residual_norm = f.lpNorm<Eigen::Infinity>();

                if (verbose_)
                {
                    std::cout << iter << "\t" << residual_norm << std::endl;
                }

                if (residual_norm < tolerance_)
                {
                    if (verbose_)
                        std::cout << "Converged in " << iter << " iterations." << std::endl;
                    return x;
                }

                // Use ColPivHouseholderQR for robustness against near-singular Jacobians
                Eigen::VectorXd delta_x = j.colPivHouseholderQr().solve(-f);

                // Update x & Check if the step is valid (NaN check)
                x += delta_x;
                if (x.hasNaN())
                {
                    throw std::runtime_error("NewtonSolver: Solution diverged (NaN detected).");
                }
            }

            throw std::runtime_error("NewtonSolver: Maximum iterations reached without convergence.");
        }

        // Setters for configuration
        void set_tolerance(double tol) { tolerance_ = tol; }
        void set_max_iterations(int max_iter) { max_iterations_ = max_iter; }
        void set_verbose(bool v) { verbose_ = v; }
    };

} // namespace cones

#endif // CONES_SOLVER_NEWTON_SOLVER_HPP
