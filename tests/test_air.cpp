#include "../src/lang/lexer.hpp"
#include "../src/lang/parser.hpp"
#include "../src/solver/newton_solver.hpp"
#include "../src/core/ideal_gas.hpp"
#include "../src/core/property_functions.hpp"
#include <iostream>
#include <iomanip>

using namespace cones;

int main() {
    try {
        System system;
        
        // 1. Setup Substances
        auto air = std::make_shared<IdealGasSubstance>("Air", 287.05, 1005.0);
        system.substance_manager().register_substance(air);

        // 2. Setup Functions
        auto temp_func = std::make_shared<GeneralPropertyFunction>("Temperature", PropertyType::TEMPERATURE, system.substance_manager());
        system.function_registry().register_function(temp_func);

        // 3. The Script
        std::string script = 
            "P := 101325\n"
            "rho := 1.225\n"
            "T.guess := 300\n"
            "T = Temperature(Air, P=P, rho=rho)\n";

        std::cout << "--- CoNES Air Test ---\n" << script << "--------------------\n" << std::endl;

        Lexer lexer(script);
        Parser parser(lexer.scan_tokens(), system);
        parser.parse();

        // 4. Solve
        NewtonSolver solver(1e-9, 50, true);
        solver.solve(system);

        // 5. Results
        auto& reg = system.registry();
        int t_idx = reg.register_variable("T");
        double t_final = reg.get_variable(t_idx).value;

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\nFinal Result:" << std::endl;
        std::cout << "Solved Temperature T = " << t_final << " [K]" << std::endl;
        std::cout << "Expectation (ISA): 101325 / (1.225 * 287.05) = 288.15 K" << std::endl;

        if (std::abs(t_final - 288.15) < 0.1) {
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
