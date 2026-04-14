#ifndef CONES_SOLVER_NEWTON_SOLVER_HPP
#define CONES_SOLVER_NEWTON_SOLVER_HPP

#include "../core/system.hpp"
#include <Eigen/Dense>
#include <iostream>
#include <stdexcept>

namespace cones {

class NewtonSolver {
    double tolerance_;
    int max_iterations_;
    bool verbose_;

public:
    explicit NewtonSolver(double tol = 1e-8, int max_iter = 50, bool verbose = false)
        : tolerance_(tol), max_iterations_(max_iter), verbose_(verbose) {}

    void solve(System& system) {
        Eigen::VectorXd f;
        Eigen::MatrixXd j;
        auto& reg = system.registry();

        if (verbose_) std::cout << "Starting Robust Newton-Raphson (Active DOF: " << reg.get_active_indices().size() << ")" << std::endl;

        for (int iter = 0; iter < max_iterations_; ++iter) {
            // Evaluate only active Jacobian
            system.evaluate(f, j);
            double current_norm = f.lpNorm<Eigen::Infinity>();

            if (verbose_) std::cout << "Iter " << iter << "\tResidual: " << current_norm << std::endl;

            if (current_norm < tolerance_) return;

            // Solve for active step direction
            Eigen::VectorXd delta_x_active = j.colPivHouseholderQr().solve(-f);

            // Backtracking line search for robustness
            double alpha = 1.0;
            const double beta = 0.5;
            bool step_found = false;
            Eigen::VectorXd x_active_orig = reg.get_active_values();

            for (int line_search_iter = 0; line_search_iter < 10; ++line_search_iter) {
                Eigen::VectorXd x_active_new = x_active_orig + alpha * delta_x_active;
                
                // Try applying values and checking residual
                reg.update_active_values(x_active_new);
                reg.apply_bounds();

                Eigen::VectorXd f_new;
                Eigen::MatrixXd j_unused;
                system.evaluate(f_new, j_unused);
                double new_norm = f_new.lpNorm<Eigen::Infinity>();

                if (new_norm < current_norm) {
                    step_found = true;
                    break;
                }
                
                alpha *= beta;
                if (verbose_) std::cout << "  Backtracking (alpha=" << alpha << ")..." << std::endl;
            }

            if (!step_found) {
                // If line search fails, we usually stay at the original or take full step
                reg.update_active_values(x_active_orig + delta_x_active);
                reg.apply_bounds();
            }
        }

        throw std::runtime_error("NewtonSolver: Max iterations reached.");
    }

    void set_verbose(bool v) { verbose_ = v; }
};

} // namespace cones

#endif
