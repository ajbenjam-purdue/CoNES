#include "lang/lexer.hpp"
#include "lang/parser.hpp"
#include "solver/newton_solver.hpp"
#include "core/ideal_gas.hpp"
#include "core/property_functions.hpp"
#include "core/version.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <chrono>

using namespace cones;

void print_help() {
    std::cout << Version::full() << " (Coupled Nonlinear Equation Solver)\n"
              << "Usage: cnes [input.cnes] [options]\n"
              << "Options:\n"
              << "  -o <file>          Write results to a file\n"
              << "  -v                 Verbose output (solver iterations)\n"
              << "  -s, --silent       Suppress the execution summary table\n"
              << "  --tol <val>        Override convergence tolerance (default: 1e-9)\n"
              << "  --max-iter <val>   Override max solver iterations (default: 100)\n"
              << "  --list-substances  List all registered substances\n"
              << "  --list-functions   List all registered functions\n"
              << "  --list-constants   List all built-in constants\n"
              << "  --version          Show the current CoNES version\n"
              << "  --help, -h         Show this help message\n" << std::endl;
}

int main(int argc, char* argv[]) {
    // 1. Initialize System & Default Environment
    System system;
    system.constant_registry().load_standard_constants();
    auto air = std::make_shared<IdealGasSubstance>("Air", 287.05, 1005.0);
    system.substance_manager().register_substance(air);
    system.function_registry().register_function(std::make_shared<GeneralPropertyFunction>("Pressure", PropertyType::PRESSURE, system.substance_manager()));
    system.function_registry().register_function(std::make_shared<GeneralPropertyFunction>("Temperature", PropertyType::TEMPERATURE, system.substance_manager()));
    system.function_registry().register_function(std::make_shared<GeneralPropertyFunction>("Enthalpy", PropertyType::ENTHALPY, system.substance_manager()));

    // 2. Argument Parsing
    std::string input_path = "";
    std::string output_path = "";
    bool verbose = false;
    bool silent = false;
    double tol_override = 1e-9;
    int max_iter_override = 100;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { print_help(); return 0; }
        if (arg == "--version") { std::cout << Version::full() << std::endl; return 0; }
        if (arg == "-v") { verbose = true; continue; }
        if (arg == "-s" || arg == "--silent") { silent = true; continue; }
        if (arg == "-o" && i + 1 < argc) { output_path = argv[++i]; continue; }
        if (arg == "--tol" && i + 1 < argc) { tol_override = std::stod(argv[++i]); continue; }
        if (arg == "--max-iter" && i + 1 < argc) { max_iter_override = std::stoi(argv[++i]); continue; }
        
        if (arg == "--list-substances") {
            std::cout << "Registered Substances:\n - Air (Ideal Gas)\n";
            return 0;
        }
        if (arg == "--list-functions") {
            std::cout << "Registered Functions:\n - Pressure\n - Temperature\n - Enthalpy\n - sin, cos, tan, log, exp\n";
            return 0;
        }
        if (arg == "--list-constants") {
            std::cout << "Built-in Constants:\n"
                      << " - CONST_GRAV: 9.80665 [m/s^2]\n"
                      << " - CONST_R:    8.31446 [J/mol*K]\n"
                      << " - STD_PRESS:  101325  [Pa]\n"
                      << " - STD_TEMP:   273.15  [K]\n"
                      << " - PI:         3.14159\n";
            return 0;
        }

        if (arg[0] != '-') input_path = arg;
    }

    if (input_path.empty()) {
        std::cerr << "Error: No input file specified." << std::endl;
        print_help();
        return 1;
    }

    try {
        // 3. Execution Pipeline
        std::ifstream file(input_path);
        if (!file.is_open()) throw std::runtime_error("Could not open file: " + input_path);
        std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        auto start_time = std::chrono::high_resolution_clock::now();

        Lexer lexer(source);
        Parser parser(lexer.scan_tokens(), system);
        parser.parse();

        NewtonSolver solver(tol_override, max_iter_override, verbose);
        solver.solve(system);

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end_time - start_time;

        // 4. Output Logic
        if (!silent || !output_path.empty()) {
            std::ostream* out = &std::cout;
            std::ofstream file_out;
            if (!output_path.empty()) {
                file_out.open(output_path);
                out = &file_out;
            }

            if (!silent) {
                *out << "\n" << std::string(50, '=') << "\n";
                *out << " " << Version::full() << " Execution Summary\n";
                *out << std::string(50, '=') << "\n";
                *out << std::left << std::setw(20) << "Variable" << std::setw(15) << "Value" << std::setw(10) << "Unit" << "State\n";
                *out << std::string(50, '-') << "\n";

                auto& reg = system.registry();
                for (size_t i = 0; i < reg.size(); ++i) {
                    const auto& v = reg.get_variable(i);
                    *out << std::left << std::setw(20) << v.name 
                         << std::setw(15) << std::fixed << std::setprecision(6) << v.value 
                         << std::setw(10) << (v.unit_name.empty() ? "-" : "[" + v.unit_name + "]")
                         << (v.is_fixed ? "Fixed" : "Solved") << "\n";
                }

                *out << std::string(50, '=') << "\n";
                *out << "Solve Time: " << duration.count() << " ms | DOF: " << system.registry().get_active_indices().size() << "\n";
                *out << std::string(50, '=') << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "\nFATAL ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
