#include "../src/lang/lexer.hpp"
#include "../src/lang/parser.hpp"
#include "../src/solver/newton_solver.hpp"
#include "../src/core/property_functions.hpp"
#include "../src/core/ideal_gas.hpp"
#include <iostream>
#include <iomanip>

using namespace cones;

int main() {
    try {
        // Use Air as the substance for testing relationships
        // Source
        std::string script = 
            "P := 101325\n"
            "T := 300\n"
            "rho.guess := 1.0\n"
            "P = Pressure(Air, T=T, rho=rho)\n";

        std::cout << "--- CoNES Relationship Test (Solving for rho) ---\n" << script << "--------------------\n" << std::endl;

        // Instantiate system, create the ideal gas, register the substance
        System system;
        auto air = std::make_shared<IdealGasSubstance>("Air", 287.05, 1005.0);
        system.substance_manager().register_substance(air);
        system.function_registry().register_function( // Load the pressure function only
            std::make_shared<GeneralPropertyFunction>("Pressure", PropertyType::PRESSURE, system.substance_manager())
        );

        // Lex the source, parse the tokens
        Lexer lexer(script);
        Parser parser(lexer.scan_tokens(), system);
        parser.parse();

        // Solution
        NewtonSolver solver(1e-9, 50, true);
        solver.solve(system);

        // Acquire the density 
        auto& reg = system.registry();
        double rho = reg.get_variable("rho").value;

        std::cout << std::fixed << std::setprecision(6);
        std::cout << "\nSolved Density rho = " << rho << std::endl;
        std::cout << "Expectation: 101325 / (287.05 * 300) = 1.176624" << std::endl;

        if (std::abs(rho - 1.176624) < 1e-5) {
            std::cout << "\nResult: SUCCESS" << std::endl;
        } else {
            std::cout << "\nResult: FAILURE" << std::endl;
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
