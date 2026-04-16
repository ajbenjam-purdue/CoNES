#include "../src/lang/lexer.hpp"
#include "../src/lang/parser.hpp"
#include "../src/solver/newton_solver.hpp"
#include <iostream>
#include <iomanip>

using namespace cones;

int main() {
    try {
        // 1. The Script
        std::string script = 
            "// Geometry Test\n"
            "r := 5 [m]\n"
            "pi := 3.14159265\n"
            "A.guess := 50\n"
            "A = pi * r^2\n";

        std::cout << "--- CoNES Script ---\n" << script << "--------------------\n" << std::endl;

        // 2. Lexing
        Lexer lexer(script);
        auto tokens = lexer.scan_tokens();

        // 3. Parsing
        System system;
        Parser parser(tokens, system);
        parser.parse();

        auto& reg = system.registry();
        std::cout << "Parser complete. Active Degrees of Freedom: " << reg.get_active_indices().size() << std::endl;
        std::cout << "Variables found: " << std::endl;
        for(size_t i=0; i < reg.size(); ++i) {
            auto v = reg.get_variable(i);
            std::cout << " - " << v.name << " (Fixed: " << (v.is_fixed ? "Yes" : "No") 
                      << ", Value: " << v.value << " [" << v.unit_name << "])" << std::endl;
        }

        // 4. Solving
        std::cout << "\nStarting Solver..." << std::endl;
        NewtonSolver solver(1e-9, 50, true);
        solver.solve(system);

        // 5. Results
        int a_idx = reg.register_variable("A");
        double a_final = reg.get_variable(a_idx).value;

        std::cout << std::fixed << std::setprecision(6);
        std::cout << "\nFinal Results:" << std::endl;
        std::cout << "Area A = " << a_final << " [m^2]" << std::endl;
        std::cout << "Expected = " << (3.14159265 * 5.0 * 5.0) << std::endl;

        if (std::abs(a_final - 78.539816) < 1e-4) {
            std::cout << "\nResult: SUCCESS" << std::endl;
        } else {
            std::cout << "\nResult: FAILURE" << std::endl;
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
