#include "../src/core/system.hpp"
#include "../src/solver/newton_solver.hpp"
#include <iostream>
#include <iomanip>

using namespace cones;

int main()
{
    try
    {
        System system;
        auto &reg = system.registry();

        // Register variables
        int x_idx = reg.register_variable("x");
        int y_idx = reg.register_variable("y");

        // Define System of Equations:
        // 1. x^2 + y^2 - 25 = 0  (Circle)
        // 2. x + y - 7 = 0       (Line)

        // Eq 1 AST
        auto x_node = std::make_shared<VariableNode>(x_idx, "x");
        auto y_node = std::make_shared<VariableNode>(y_idx, "y");

        auto x_sq = std::make_shared<PowNode>(x_node, 2.0);
        auto y_sq = std::make_shared<PowNode>(y_node, 2.0);
        auto sum_sq = std::make_shared<AddNode>(x_sq, y_sq);
        auto eq1 = std::make_shared<SubNode>(sum_sq, std::make_shared<ConstantNode>(25.0));

        // Eq 2 AST
        auto sum_xy = std::make_shared<AddNode>(x_node, y_node);
        auto eq2 = std::make_shared<SubNode>(sum_xy, std::make_shared<ConstantNode>(7.0));

        system.add_equation(eq1);
        system.add_equation(eq2);

        std::cout << "Solving System:" << std::endl;
        std::cout << "1. " << eq1->to_string() << " = 0" << std::endl;
        std::cout << "2. " << eq2->to_string() << " = 0" << std::endl;

        // Solver Configuration
        NewtonSolver solver;
        solver.set_verbose(true);
        solver.set_tolerance(1e-9);

        // Initial Guess (Starting near one of the solutions)
        Eigen::VectorXd guess(2);
        guess << 2.0, 5.0; // Near (3, 4)

        std::cout << "\nInitial Guess: x=2.0, y=5.0" << std::endl;

        Eigen::VectorXd solution = solver.solve(system, guess);

        std::cout << std::fixed << std::setprecision(8);
        std::cout << "\nFinal Solution:" << std::endl;
        std::cout << "x = " << solution(x_idx) << std::endl;
        std::cout << "y = " << solution(y_idx) << std::endl;

        // Validation
        double res1 = std::pow(solution(x_idx), 2) + std::pow(solution(y_idx), 2) - 25.0;
        double res2 = solution(x_idx) + solution(y_idx) - 7.0;

        std::cout << "\nFinal Residuals:" << std::endl;
        std::cout << "Eq 1 Residual: " << res1 << std::endl;
        std::cout << "Eq 2 Residual: " << res2 << std::endl;

        if (std::abs(res1) < 1e-7 && std::abs(res2) < 1e-7)
        {
            std::cout << "\nResult: SUCCESS" << std::endl;
        }
        else
        {
            std::cout << "\nResult: FAILURE (Residuals too high)" << std::endl;
            return 1;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
