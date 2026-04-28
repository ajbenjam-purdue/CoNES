#ifndef CONES_SOLVER_NEWTON_SOLVER_HPP
#define CONES_SOLVER_NEWTON_SOLVER_HPP

#include "../core/system.hpp"
#include <Eigen/Dense>
#include <iostream>
#include <stdexcept>
#include <random>
#include <cmath>

namespace cones {

class NewtonSolver {
    double tolerance_;
    int max_iterations_;
    bool verbose_;

public:
    explicit NewtonSolver(double tol = 1e-8, int max_iter = 200, bool verbose = false)
        : tolerance_(tol), max_iterations_(max_iter), verbose_(verbose) {}

    void solve(System& system) {
        try {
            solve_internal(system);
            return;
        } catch (const std::exception& e) {
            if (verbose_) std::cout << "Initial solve failed: " << e.what() << ". Starting Multistart..." << std::endl;
        }

        std::mt19937 gen(1337);
        auto& reg = system.registry();
        Eigen::VectorXd original_x = reg.get_active_values();

        for (int attempt = 0; attempt < 3; ++attempt) {
            if (verbose_) std::cout << "Attempt " << attempt + 1 << "/3..." << std::endl;
            
            double jitter_range = 0.1 * (attempt + 1);
            std::uniform_real_distribution<> dis(1.0 - jitter_range, 1.0 + jitter_range);
            
            Eigen::VectorXd jittered_x = original_x;
            for (int i = 0; i < jittered_x.size(); ++i) {
                jittered_x(i) *= dis(gen);
                if (std::abs(jittered_x(i)) < 1e-6) jittered_x(i) = (dis(gen) - 1.0) * 100.0;
            }
            
            reg.update_active_values(jittered_x);
            reg.apply_bounds();

            try {
                solve_internal(system);
                return;
            } catch (...) {
                continue;
            }
        }

        throw std::runtime_error("NewtonSolver: Failed to converge after multiple attempts.");
    }

private:
    void solve_internal(System& system) {
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

            if (current_inf_norm < tolerance_) return;

            // Robust LM step: Solve (J^T J + lambda * I) dx = -J^T f
            // But we'll use a more stable QR approach on augmented system:
            // [ J             ] [ dx ] = [ -f ]
            // [ sqrt(lambda)*I ] [    ]   [  0 ]
            
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
                max_change = std::max(max_change, std::abs(delta_x(i)) / (std::abs(x_orig(i)) + 1.0));
            }
            if (max_change > 0.5) delta_x *= (0.5 / max_change);

            // Backtracking
            double alpha = 1.0;
            bool success = false;
            for (int ls = 0; ls < 10; ++ls) {
                reg.update_active_values(x_orig + alpha * delta_x);
                reg.apply_bounds();
                
                Eigen::VectorXd f_new;
                Eigen::MatrixXd j_unused;
                system.evaluate(f_new, j_unused);
                double new_l2_norm = f_new.norm();
                
                if (new_l2_norm < current_l2_norm) {
                    success = true;
                    lambda = std::max(lambda_min, lambda * 0.1);
                    break;
                }
                alpha *= 0.5;
            }

            if (!success) {
                lambda = std::min(lambda_max, lambda * 10.0);
                Eigen::VectorXd jittered = x_orig;
                for(int i=0; i<jittered.size(); ++i) {
                    jittered(i) += dis(gen) * (std::abs(jittered(i)) * 0.01 + 0.01);
                }
                reg.update_active_values(jittered);
                reg.apply_bounds();
            }

            if (verbose_ && iter % 10 == 0) {
                std::cout << "  Iter " << iter << " Resid: " << current_inf_norm << " (L=" << lambda << ")" << std::endl;
            }
        }
        throw std::runtime_error("Max iterations reached");
    }

public:
    void set_verbose(bool v) { verbose_ = v; }
};

} // namespace cones

#endif
