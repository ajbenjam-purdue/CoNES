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

        // Try initial guess first
        if (solve_internal(system, report)) return report;

        // Multistart/Scrambling
        std::mt19937 gen(42);
        std::uniform_real_distribution<> dis(-1.0, 1.0);

        for (int attempt = 1; attempt < max_attempts_; ++attempt)
        {
            if (verbose_) std::cout << "Attempt " << attempt + 1 << "/" << max_attempts_ << "..." << std::endl;
            
            Eigen::VectorXd active_vals = reg.get_active_values();
            for (int i = 0; i < active_vals.size(); ++i)
            {
                double range = 1.0; 
                active_vals(i) += dis(gen) * range * (std::abs(active_vals(i)) + 1.0);
            }
            reg.update_active_values(active_vals);
            reg.apply_bounds();

            if (solve_internal(system, report)) return report;
        }

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

            // Original Solver Logic
            Eigen::MatrixXd AtA = j.transpose() * j;
            Eigen::VectorXd Atf = j.transpose() * f;
            
            for(int i=0; i<m; ++i) AtA(i,i) += lambda * (AtA(i,i) + 1.0);

            Eigen::VectorXd delta_x = AtA.ldlt().solve(-Atf);

            double max_change = 0.0;
            Eigen::VectorXd x_orig = reg.get_active_values();
            for(int i=0; i < delta_x.size(); ++i) {
                max_change = std::max(max_change, std::abs(delta_x(i)) / (std::abs(x_orig(i)) + 1e-3));
            }
            if (max_change > 0.1) delta_x *= (0.1 / max_change);

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
                
                // If the result is improved, we keep it
                if (new_l2_norm < current_l2_norm) { 
                    success = true;
                    lambda = std::max(lambda_min, lambda * 0.1);
                    break;
                }
                alpha *= 0.25; // Quarter the step size for every failure
            }

            if (!success) {
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
