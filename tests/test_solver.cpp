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

        // 1. Register and configure variables (EES-style)
        int x_idx = reg.register_variable("x");
        int y_idx = reg.register_variable("y");

        reg.set_value(x_idx, 2.0);        // Initial guess
        reg.set_value(y_idx, 5.0);        // Initial guess
        reg.set_bounds(x_idx, 0.0, 10.0); // Physical limits
        reg.set_bounds(y_idx, 0.0, 10.0);

        // 2. Build equations
        auto x_node = std::make_shared<VariableNode>(x_idx, "x");
        auto y_node = std::make_shared<VariableNode>(y_idx, "y");

        // Eq 1: x^2 + y^2 = 25
        auto x_sq = std::make_shared<PowNode>(x_node, 2.0);
        auto y_sq = std::make_shared<PowNode>(y_node, 2.0);
        auto sum_sq = std::make_shared<AddNode>(x_sq, y_sq);
        auto eq1 = std::make_shared<SubNode>(sum_sq, std::make_shared<ConstantNode>(25.0));

        // Eq 2: x + y = 7
        auto sum_xy = std::make_shared<AddNode>(x_node, y_node);
        auto eq2 = std::make_shared<SubNode>(sum_xy, std::make_shared<ConstantNode>(7.0));

        system.add_equation(eq1);
        system.add_equation(eq2);

        std::cout << "Solving System with Robust Newton Solver:" << std::endl;
        std::cout << "1. " << eq1->to_string() << " = 0" << std::endl;
        std::cout << "2. " << eq2->to_string() << " = 0" << std::endl;

        // 3. Solve
        NewtonSolver solver(1e-9, 50, true);
        solver.solve(system);

        // 4. Extract results from Registry
        double x_final = reg.get_variable(x_idx).value;
        double y_final = reg.get_variable(y_idx).value;

        std::cout << std::fixed << std::setprecision(8);
        std::cout << "\nFinal Solution:" << std::endl;
        std::cout << "x = " << x_final << std::endl;
        std::cout << "y = " << y_final << std::endl;

        // 5. Validation
        double res1 = std::pow(x_final, 2) + std::pow(y_final, 2) - 25.0;
        double res2 = x_final + y_final - 7.0;

        std::cout << "\nFinal Residuals:" << std::endl;
        std::cout << "Eq 1 Residual: " << res1 << std::endl;
        std::cout << "Eq 2 Residual: " << res2 << std::endl;

        if (std::abs(res1) < 1e-7 && std::abs(res2) < 1e-7)
        {
            std::cout << "\nResult: SUCCESS" << std::endl;
        }
        else
        {
            std::cout << "\nResult: FAILURE" << std::endl;
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
