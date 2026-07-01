#ifndef CONES_SOLVER_NEWTON_SOLVER_HPP
#define CONES_SOLVER_NEWTON_SOLVER_HPP

#include "../core/system.hpp"
#include <Eigen/Dense>
#include <iostream>
#include <stdexcept>
#include <random>
#include <cmath>
#include <cstdio>

namespace cones
{

struct SolverReport
{
    bool success = false;
    std::string error_msg = "";
    int iterations = 1; // We gotta start with one
    Eigen::VectorXd residuals;
};

class NewtonSolver
{
    double tolerance_;
    int max_iterations_;
    int max_attempts_ = 100;
    bool verbose_;

public:
    explicit NewtonSolver(double tol = 1e-9, int max_iter = 1000, bool verbose = false)
        : tolerance_(tol), max_iterations_(max_iter), verbose_(verbose) {}

    SolverReport solve(const System &system)
    {
        auto &reg = const_cast<System &>(system).registry();
        SolverReport report;

        // Store original guesses
        Eigen::VectorXd original_guesses = reg.get_active_values();

        // Try initial guess first
        if (solve_internal(system, report)) return report;

        // Multistart/Scrambling
        std::mt19937 gen(42);
        std::uniform_real_distribution<> dis(-1.0, 1.0);

        for (int attempt = 1; attempt < max_attempts_; ++attempt)
        {
            if (verbose_) std::cout << "Attempt " << attempt + 1 << "/" << max_attempts_ << "..." << std::endl;
            
            // Perturb from the original guess values
            Eigen::VectorXd active_vals = original_guesses;
            for (int i = 0; i < active_vals.size(); ++i)
            {
                double range = 1.0; 
                active_vals(i) += dis(gen) * range * (std::abs(active_vals(i)) + 1.0);
            }
            reg.update_active_values(active_vals);
            reg.apply_bounds();

            if (solve_internal(system, report)) return report;
        }

        // Restore original guesses if we failed
        reg.update_active_values(original_guesses);
        reg.apply_bounds();

        report.success = false;
        report.error_msg = "NewtonSolver: Failed to converge after " + std::to_string(max_attempts_) + " attempts.";
        return report;
    }

private:
    bool solve_internal(const System &system, SolverReport &report)
    {
        auto &reg = const_cast<System &>(system).registry();
        int n = static_cast<int>(system.get_equation_count());
        auto active_indices = reg.get_active_indices();
        int m = static_cast<int>(active_indices.size());

        if (verbose_) {
            std::cout << "--- Variables before solve ---" << std::endl;
            for (size_t i = 0; i < reg.size(); ++i) {
                const auto &var = reg.get_variable(i);
                std::cout << "Var " << i << ": " << var.name << " = " << var.value 
                          << " [" << var.unit_name << "] bounds=[" << var.lower_bound << ", " << var.upper_bound << "]" << std::endl;
            }
            std::cout << "--- Equations before solve ---" << std::endl;
            for (size_t i = 0; i < system.get_equation_count(); ++i) {
                std::cout << "Eq " << i << ": " << system.get_equation_plaintext(i) << std::endl;
            }
        }

        if (n == 0) { report.success = true; return true; }

        Eigen::VectorXd f(n);
        Eigen::MatrixXd j(n, m);

        double lambda = 1e-7;
        const double lambda_min = 1e-12;
        const double lambda_max = 1e3;

        std::mt19937 gen(1337);
        std::uniform_real_distribution<> dis(-1.0, 1.0);

        for (int iter = 0; iter < max_iterations_; ++iter)
        {
            system.evaluate(f, j);
            double current_inf_norm = f.lpNorm<Eigen::Infinity>();

            report.iterations = std::max(iter, report.iterations);
            if (current_inf_norm < tolerance_)
            {
                report.success = true;
                report.residuals = f;
                return true;
            }

            // Jacobi preconditioning: scale columns of the Jacobian to handle ill-conditioned systems
            Eigen::VectorXd col_norms(m);
            for (int i = 0; i < m; ++i) {
                col_norms(i) = std::max(j.col(i).norm(), 1e-4);
            }

            Eigen::MatrixXd j_scaled = j;
            for (int i = 0; i < m; ++i) {
                j_scaled.col(i) /= col_norms(i);
            }

            Eigen::MatrixXd AtA = j_scaled.transpose() * j_scaled;
            Eigen::VectorXd Atf = j_scaled.transpose() * f;
            
            for(int i=0; i<m; ++i) AtA(i,i) += lambda * (AtA(i,i) + 1.0);

            Eigen::VectorXd delta_y = AtA.colPivHouseholderQr().solve(-Atf);

            // Unscale step back to normal variables
            Eigen::VectorXd delta_x(m);
            for (int i = 0; i < m; ++i) {
                delta_x(i) = delta_y(i) / col_norms(i);
            }

            Eigen::VectorXd x_orig = reg.get_active_values();
            for(int i=0; i < delta_x.size(); ++i) {
                double val = x_orig(i);
                double step = delta_x(i);
                int global_idx = active_indices[i];
                const auto &var = reg.get_variable(global_idx);
                if (var.lower_bound > 0) {
                    // Prevent it from dropping too low (no more than 80% decrease)
                    if (step < 0 && val + step < 0.2 * val) {
                        delta_x(i) = -0.8 * val;
                    }
                    // Prevent it from growing too fast (no more than 5x increase)
                    if (step > 0 && val + step > 5.0 * val) {
                        delta_x(i) = 4.0 * val;
                    }
                } else {
                    // General variables: limit step to 5x of value + 1000.0 to allow growth from 0
                    double limit = 5.0 * (std::abs(val) + 1000.0);
                    if (std::abs(step) > limit) {
                        delta_x(i) = (step > 0 ? 1.0 : -1.0) * limit;
                    }
                }
            }

            double alpha = 1.0;
            bool success = false;
            double current_l2_norm = f.norm();

            for (int ls = 0; ls < 5; ++ls) {
                // We have stored the original values, so let's try with some shifted values
                reg.update_active_values(x_orig + alpha * delta_x);
                reg.apply_bounds(); // Reinforce the bounds
                
                Eigen::VectorXd f_new;
                Eigen::MatrixXd j_unused; // Unused
                system.evaluate(f_new, j_unused);
                double new_l2_norm = f_new.norm();
                
                // Relaxed non-monotone line search: accept step if norm decreases, is close to solved, or increases by at most 10%
                if (new_l2_norm < current_l2_norm * 1.1 || new_l2_norm < tolerance_) { 
                    success = true;
                    if (verbose_) std::cout << "    LS success at step=" << ls << " alpha=" << alpha << " ratio=" << new_l2_norm / current_l2_norm << std::endl;
                    lambda = std::max(lambda_min, lambda * 0.1);
                    break;
                }
                alpha *= 0.25; // Quarter the step size for every failure
            }

            if (!success) {
                if (verbose_) std::cout << "    LS failed! No improvement found." << std::endl;
                lambda = std::min(lambda_max, lambda * 10.0);
                Eigen::VectorXd jittered = x_orig;
                for(int i=0; i<jittered.size(); ++i) {
                    jittered(i) += dis(gen) * (std::abs(jittered(i)) * 0.01 + 0.01); // TODO: Abstract and refine this
                }
                reg.update_active_values(jittered);
                reg.apply_bounds();
            }

            if (verbose_ && iter % 20 == 0) {
                std::cout << "  Iter " << iter << " Resid: " << current_inf_norm << " (L=" << lambda << ")" << std::endl;
                for (int i = 0; i < n; ++i) {
                    if (std::abs(f(i)) > tolerance_) {
                        std::cout << "    Eq " << i << " (" << system.get_equation_plaintext(i) << ") = " << f(i) << std::endl;
                    }
                }
            }
            report.residuals = f;
        }

        return false;
    }

public:
    void set_verbose(bool v) { verbose_ = v; }
    void set_max_scramble_attempts(int i) {
        max_attempts_ = (i > 100 ? 100 : (i < 1 ? 1 : i));
    }
};

} // namespace cones

#endif
