#ifndef CONES_SOLVER_NEWTON_SOLVER_HPP
#define CONES_SOLVER_NEWTON_SOLVER_HPP

#include "../core/system.hpp"
#include <Eigen/Dense>
#include <iostream>
#include <stdexcept>
#include <random>
#include <cmath>
#include <cstdio>

namespace cones {

struct SolverReport {
    int iterations = 0;
    bool success = false;
    Eigen::VectorXd residuals;
    std::string error_msg = "";
};

class NewtonSolver {
    double tolerance_;
    int max_iterations_;
    int max_attempts_ = 100;
    bool verbose_;

public:
    explicit NewtonSolver(double tol = 1e-7, int max_iter = 1000, bool verbose = false)
        : tolerance_(tol), max_iterations_(max_iter), verbose_(verbose) {}

    SolverReport solve(System& system) { 
        SolverReport report;
        
        // Initial solve attempt
        solve_internal(system, report);
        if (report.success)
            return report;
        
        if (verbose_) 
            std::cout << "Initial solve failed. Starting Multistart..." << std::endl;

        std::mt19937 gen(1337);
        auto& reg = system.registry();
        Eigen::VectorXd original_x = reg.get_active_values();

        for (int attempt = 0; attempt < max_attempts_; ++attempt) {
            if (verbose_) std::cout << "Attempt " << attempt + 1 << "/" << std::to_string(max_attempts_) << "..." << std::endl;
            
            double jitter_range = 0.2 * (attempt + 1);
            std::uniform_real_distribution<> dis(1.0 - jitter_range, 1.0 + jitter_range);

            Eigen::VectorXd jittered_x = original_x;
            for (int i = 0; i < jittered_x.size(); ++i) {
                jittered_x(i) *= dis(gen);
                if (std::abs(jittered_x(i)) < 1e-6) jittered_x(i) = (dis(gen) - 1.0) * 100.0;
            }
            
            reg.update_active_values(jittered_x);
            reg.apply_bounds();

            SolverReport sub_report;
            solve_internal(system, sub_report);
            report.iterations += sub_report.iterations;
            
            if (sub_report.success) {
                report.success = true;
                report.residuals = sub_report.residuals;
                return report;
            }
        }

        if (verbose_) {
            std::cout << "Final state before failure:" << std::endl;
            for (int i = 0; i < reg.size(); ++i) {
                const auto& v = reg.get_variable(i);
                std::cout << "  " << v.name << " = " << v.value << " [" << v.unit_name << "]" << std::endl;
            }
        }
        report.error_msg = "NewtonSolver: Failed to converge after " + std::to_string(max_attempts_) + " attempts.";
        return report;
    }

private:
    void solve_internal(System& system, SolverReport& report) {
        Eigen::VectorXd f;
        Eigen::MatrixXd j;
        auto& reg = system.registry();
        std::mt19937 gen(42);
        std::uniform_real_distribution<> dis(-0.02, 0.02);
        
        double lambda = 1e-7; 
        const double lambda_min = 1e-12;
        const double lambda_max = 1e3;

        for (int iter = 0; iter < max_iterations_; ++iter) {
            system.evaluate(f, j);
            double current_inf_norm = f.lpNorm<Eigen::Infinity>();
            double current_l2_norm = f.norm();

            if (current_inf_norm < tolerance_) {
                report.success = true;
                report.iterations = iter;
                report.residuals = f;
                return;
            }

            // Normalization

            int n = static_cast<int>(f.size());
            int m = static_cast<int>(j.cols());

            // Row normalization (Balancing)
            for(int i=0; i<n; ++i) {
                double row_norm = j.row(i).norm();
                if (row_norm > 1e-9) {
                    j.row(i) /= row_norm; // Jacobian
                    f(i) /= row_norm;     // Resid
                }
            }

            // Solve (J^T J + lambda * I) dx = -J^T f
            // [ J              ]  dx  = [ -f ]
            // [ sqrt(lambda)*I ]        [  0 ]
            
            Eigen::MatrixXd A(n + m, m);
            Eigen::VectorXd b(n + m);
            
            A.block(0, 0, n, m) = j;
            b.segment(0, n) = -f;
            
            double diag_lambda = std::sqrt(lambda);
            A.block(n, 0, m, m).setZero();
            for(int i=0; i<m; ++i) {
                double col_norm = j.col(i).norm();
                A(n+i, i) = diag_lambda * (col_norm + 1.0); 
            }
            b.segment(n, m).setZero();

            Eigen::VectorXd delta_x = A.colPivHouseholderQr().solve(b);

            // Step limiting
            double max_change = 0.0;
            Eigen::VectorXd x_orig = reg.get_active_values();
            for(int i=0; i < delta_x.size(); ++i) {
                // Use a larger floor for the relative change calculation to avoid tiny steps for small variables
                max_change = std::max(max_change, std::abs(delta_x(i)) / (std::abs(x_orig(i)) + 1e-3));
            }
            if (max_change > 0.1) delta_x *= (0.1 / max_change);

            // Backtracking
            double alpha = 1.0;
            bool success = false;
            for (int ls = 0; ls < 5; ++ls) {
                // We have stored the original values, so let's try with some shifted values
                reg.update_active_values(x_orig + alpha * delta_x);
                reg.apply_bounds(); // Reinforce the bounds
                
                Eigen::VectorXd f_new;
                Eigen::MatrixXd j_unused; // Unused
                system.evaluate(f_new, j_unused);
                double new_l2_norm = f_new.norm();
                
                // If the result is improved, we keep it
                if (new_l2_norm < current_l2_norm) { 
                    success = true;
                    lambda = std::max(lambda_min, lambda * 0.1);
                    break;
                }
                alpha *= 0.25; // Quarter the step size for every failure
            }

            if (verbose_ && iter % 20 == 0) { // Print more detail every 20 iters
                std::cout << "  Iter " << iter << " Resid: " << current_inf_norm << " (L=" << lambda << ")" << std::endl;
                for (int i = 0; i < f.size(); ++i) {
                    if (std::abs(f(i)) > tolerance_) {
                        std::cout << "    Eq " << i << " (" << system.get_equation_plaintext(i) << ") Resid: " << f(i) << std::endl;
                    }
                }
            }

            // No sub-iters resulted in an improvement (instability)
            // Pseudo scramble all our values
            if (!success) {
                lambda = std::min(lambda_max, lambda * 10.0);
                Eigen::VectorXd jittered = x_orig;
                for(int i=0; i<jittered.size(); ++i) {
                    jittered(i) += dis(gen) * (std::abs(jittered(i)) * 0.01 + 0.01); // TODO: Abstract and refine this
                }
                reg.update_active_values(jittered);
                reg.apply_bounds();
            }
        }
        report.success = false;
        report.iterations = max_iterations_;
        report.residuals = f;
    }

public:
    void set_verbose(bool v) { verbose_ = v; } // Set verbose to the provided boolean value
    void set_verbose() { verbose_ = true; }    // Override verbosity to true
    void set_max_scramble_attempts(int i) { // Set the maximum solver-scramble attempts (1 - 100, incl.)
        max_attempts_ = (i > 100 ? 100 : (i < 1 ? 1 : i));
    }
};

} // namespace cones

#endif
