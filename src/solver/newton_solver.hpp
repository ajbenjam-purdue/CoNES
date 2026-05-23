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

class NewtonSolver {
    double tolerance_;
    int max_iterations_;
    int max_attempts_ = 5;
    bool verbose_;

public:
    explicit NewtonSolver(double tol = 1e-8, int max_iter = 500, bool verbose = false)
        : tolerance_(tol), max_iterations_(max_iter), verbose_(verbose) {}

    int solve(System& system) { // Yields the count of iterations needed to converge to the system's tolerance OR -1 if tolerance is not reached

        // TODO: Fix this architecture. It currently shifts guesses randomly both within solve_internal and outside. 
        // Should be just one method with optional parameters
        
        int iters = solve_internal(system);
        if (iters != -1)
            return iters;
        else if (verbose_) 
            std::cout << "Initial solve failed. Starting Multistart..." << std::endl;
        

        std::mt19937 gen(1337);
        auto& reg = system.registry();
        Eigen::VectorXd original_x = reg.get_active_values();

        for (int attempt = 0; attempt < max_attempts_; ++attempt) {
            if (verbose_) std::cout << "Attempt " << attempt + 1 << "/" << std::to_string(max_attempts_) << "..." << std::endl;
            
            double jitter_range = 0.1 * (attempt + 1); // Step up the jitter as more failures occur
            std::uniform_real_distribution<> dis(1.0 - jitter_range, 1.0 + jitter_range);

            // Scramble
            Eigen::VectorXd jittered_x = original_x;
            for (int i = 0; i < jittered_x.size(); ++i) {
                jittered_x(i) *= dis(gen);
                if (std::abs(jittered_x(i)) < 1e-6) jittered_x(i) = (dis(gen) - 1.0) * 100.0;
            }
            
            reg.update_active_values(jittered_x);
            reg.apply_bounds();

            int iters_additional = solve_internal(system);
            if (iters_additional != -1) // Good solution! Exit
            {
                return iters + iters_additional;
            }
            else
            {
                iters += iters_additional;
                continue;
            }
        }

        if (verbose_) {
            std::cout << "Final state before failure:" << std::endl;
            for (int i = 0; i < reg.size(); ++i) {
                const auto& v = reg.get_variable(i);
                std::cout << "  " << v.name << " = " << v.value << " [" << v.unit_name << "]" << std::endl;
            }
        }
        throw std::runtime_error("NewtonSolver: Failed to converge after " + std::to_string(max_attempts_) + " attempts.");
    }

private:
    int solve_internal(System& system) { // Yields the count of iterations needed to converge to the system's tolerance OR -1 if tolerance is not reached
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

            if (current_inf_norm < tolerance_) return iter;

            // Solve (J^T J + lambda * I) dx = -J^T f
            // Instead of directly trying to deal with transposition and inversions, we attempt a more conservative approach:
            // [ J              ]  dx  = [ -f ]
            // [ sqrt(lambda)*I ]        [  0 ]
            
            int n = static_cast<int>(f.size());
            int m = static_cast<int>(j.cols());
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
                max_change = std::max(max_change, std::abs(delta_x(i)) / (std::abs(x_orig(i)) + 1e3));
            }
            if (max_change > 10.0) delta_x *= (10.0 / max_change);

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

            if (verbose_ && iter % 10 == 0) { // Print every tenth run (verb only)
                std::cout << "  Iter " << iter << " Resid: " << current_inf_norm << " (L=" << lambda << ")" << std::endl;
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
        return -1;
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
