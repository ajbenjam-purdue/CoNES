#define _USE_MATH_DEFINES
#include "../src/core/expression.hpp"
#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>

using namespace cones;

int main()
{
    // Equation: f(x, y) = sin(x) * y + x^2
    // Indices: x -> 0, y -> 1

    // Build the AST manually (mimicking what the parser will eventually do)
    auto x_node = std::make_shared<VariableNode>(0, "x");
    auto y_node = std::make_shared<VariableNode>(1, "y");

    auto sin_x = std::make_shared<SinNode>(x_node);
    auto term1 = std::make_shared<MulNode>(sin_x, y_node);
    auto term2 = std::make_shared<PowNode>(x_node, 2.0);

    auto root = std::make_shared<AddNode>(term1, term2);

    std::cout << "Testing Equation: " << root->to_string() << std::endl;

    // Test values: x = PI/2, y = 2.0
    double x_val = M_PI / 2.0;
    double y_val = 2.0;

    // To get df/dx:
    // x = {x_val, 1.0} (seed derivative = 1)
    // y = {y_val, 0.0} (seed derivative = 0)
    std::vector<DualNumber> vals_for_dx = {{x_val, 1.0}, {y_val, 0.0}};
    DualNumber result_dx = root->evaluate(vals_for_dx);

    // To get df/dy:
    // x = {x_val, 0.0}
    // y = {y_val, 1.0}
    std::vector<DualNumber> vals_for_dy = {{x_val, 0.0}, {y_val, 1.0}};
    DualNumber result_dy = root->evaluate(vals_for_dy);

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "\nAt x = " << x_val << ", y = " << y_val << ":" << std::endl;
    std::cout << "f(x, y)    = " << result_dx.val << " (Expected: sin(PI/2)*2 + (PI/2)^2 = 1*2 + 2.467 = 4.467401)" << std::endl;
    std::cout << "df/dx      = " << result_dx.der << " (Expected: cos(x)*y + 2x = 0*2 + 2*(PI/2) = PI = 3.141593)" << std::endl;
    std::cout << "df/dy      = " << result_dy.der << " (Expected: sin(x) = sin(PI/2) = 1.000000)" << std::endl;

    // Verification
    bool success = std::abs(result_dx.val - 4.467401) < 1e-5 &&
                   std::abs(result_dx.der - 3.141593) < 1e-5 &&
                   std::abs(result_dy.der - 1.0) < 1e-5;

    if (success)
    {
        std::cout << "\nResult: SUCCESS" << std::endl;
    }
    else
    {
        std::cout << "\nResult: FAILURE" << std::endl;
        return 1;
    }

    return 0;
}
