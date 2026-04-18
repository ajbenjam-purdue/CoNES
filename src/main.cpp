#include "lang/lexer.hpp"
#include "lang/parser.hpp"
#include "solver/newton_solver.hpp"
#include "core/ideal_gas.hpp"
#include "core/property_functions.hpp"
#include "core/tabulated_substance.hpp"
#include "core/version.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <chrono>
#include <filesystem>

using namespace cones;

void print_help() {
    // std::cout << Version::full() << " (Coupled Nonlinear Equation Solver)\n"
    //           << "Usage: cnes [input.cnes] [options]\n"
    //           << "Options:\n"
    //           << "  -o <file>          Write results to a file\n"
    //           << "  -v                 Verbose output (solver iterations)\n"
    //           << "  -s, --silent       Suppress the execution summary table\n"
    //           << "  --tol <val>        Override convergence tolerance (default: 1e-9)\n"
    //           << "  --max-iter <val>   Override max solver iterations (default: 100)\n"
    //           << "  --list-substances  List all registered substances\n"
    //           << "  --out-substances   Write current substances available to the output stream, separated by pipes.\n"
    //           << "  --list-functions   List all registered functions\n"
    //           << "  --out-functions    Write current functions available to the output stream, separated by pipes.\n"
    //           << "  --list-constants   List all built-in constants\n"
    //           << "  --out-constants    Write current constants available to the output stream, separated by pipes. If -v is passed after, includes descriptions (CONST_A:description A|...)\n"
    //           << "  --version          Show the current CoNES version\n"
    //           << "  --help, -h         Show this help message\n" << std::endl;

    std::cout << Version::full() << " (Coupled Nonlinear Equation Solver)\n"
              << "Usage: cnes [input.cnes] [options]\n\n"
              << "Options:\n\n"
              << "  Output:\n"
              << "    -o <file>          Write results to a file\n"
              << "    -v                 Verbose output (solver iterations)\n"
              << "    -s, --silent       Suppress the execution summary table\n\n"
              << "  Solver:\n"
              << "    --tol <val>        Override convergence tolerance (default: 1e-9)\n"
              << "    --max-iter <val>   Override max solver iterations (default: 100)\n\n"
              << "  Development Tools:\n"
              << "    --list-substances  List all registered substances\n"
              << "    --list-functions   List all registered functions\n"
              << "    --list-constants   List all built-in constants\n"
              << "    --out-substances   Write current substances available to the output stream, separated by pipes.\n"
              << "    --out-functions    Write current functions available to the output stream, separated by pipes.\n"
              << "    --out-constants    Write current constants available to the output stream, separated by pipes. If -v is passed after, includes descriptions (CONST_A:description A|...)\n\n"
              << "  General:\n"
              << "    --version          Show the current CoNES version\n"
              << "    --help, -h         Show this help message\n" << std::endl;
}

int main(int argc, char* argv[]) {
    // 1. Initialize System & Default Environment
    System system;
    system.constant_registry().load_standard_constants();

    // Get executable directory for portable material loading
    std::filesystem::path exe_path = std::filesystem::absolute(argv[0]).parent_path();
    std::filesystem::path materials_path = exe_path / "materials";

    auto air = std::make_shared<IdealGasSubstance>("Air", 287.05, 1005.0);
    system.substance_manager().register_substance(air);

    // Automatically load Tabulated Substances from /materials relative to exe
    if (std::filesystem::exists(materials_path)) {
        for (const auto& entry : std::filesystem::directory_iterator(materials_path)) {
            if (entry.path().extension() == ".cnesbin") {
                std::string fname = entry.path().stem().string(); // e.g., "Water_h"
                std::string sub_name = fname.substr(0, fname.find('_'));
                
                auto sub = std::dynamic_pointer_cast<TabulatedSubstance>(system.substance_manager().get(sub_name));
                if (!sub) {
                    sub = std::make_shared<TabulatedSubstance>(sub_name);
                    system.substance_manager().register_substance(sub);
                }
                sub->load_table(entry.path().string());
            }
        }
    }

    // Register Property Functions
    system.function_registry().register_function(std::make_shared<GeneralPropertyFunction>("Pressure", PropertyType::PRESSURE, system.substance_manager(), "Yields the pressure in Pascals of the substance at the provided state."));
    system.function_registry().register_function(std::make_shared<GeneralPropertyFunction>("Temperature", PropertyType::TEMPERATURE, system.substance_manager(), "Yields the temperature in Kelvin of the substance at the provided state."));
    system.function_registry().register_function(std::make_shared<GeneralPropertyFunction>("Enthalpy", PropertyType::ENTHALPY, system.substance_manager(), "Yields the specific enthalpy in J/kg of the substance at the provided state."));

    // Register Math Functions
    system.function_registry().register_function(std::make_shared<MathFunction>("sin", "x", "Trigonometric sine (argument in radians)."));
    system.function_registry().register_function(std::make_shared<MathFunction>("cos", "x", "Trigonometric cosine (argument in radians)."));
    system.function_registry().register_function(std::make_shared<MathFunction>("tan", "x", "Trigonometric tangent (argument in radians)."));
    system.function_registry().register_function(std::make_shared<MathFunction>("sqrt", "x", "Square root of a non-negative number."));
    system.function_registry().register_function(std::make_shared<MathFunction>("log", "x", "Natural logarithm (base-e)."));
    system.function_registry().register_function(std::make_shared<MathFunction>("exp", "x", "Exponential function (e^x)."));

    // 2. Argument Parsing
    std::string input_path = "";
    std::string output_path = "";
    bool verbose = false;
    bool silent = false;
    double tol_override = 1e-9;
    int max_iter_override = 100;

    for (int i = 1; i < argc; ++i) {
        std::string flag = argv[i];
        if (flag == "--help" || flag == "-h") { print_help(); return 0; }
        if (flag == "--version") { std::cout << Version::full() << std::endl; return 0; }
        if (flag == "-v") { verbose = true; continue; }
        if (flag == "-s" || flag == "--silent") { silent = true; continue; }
        if (flag == "-o" && i + 1 < argc) { output_path = argv[++i]; continue; }
        if (flag == "--tol" && i + 1 < argc) { tol_override = std::stod(argv[++i]); continue; }
        if (flag == "--max-iter" && i + 1 < argc) { max_iter_override = std::stoi(argv[++i]); continue; }
        
        if (flag == "--list-substances") {
            std::cout << "Registered Substances:\n";
            for (const auto& name : system.substance_manager().get_substance_names()) {
                std::cout << " - " << name << "\n";
            }
            return 0;
        }
        if (flag == "--out-substances") {
            auto names = system.substance_manager().get_substance_names();
            for (size_t i = 0; i < names.size(); ++i) {
                std::cout << names[i] << (i < names.size() - 1 ? "|" : "");
            }
            return 0;
        }
        if (flag == "--list-functions") {
            std::cout << "Registered Functions:\n";
            for (const auto& name : system.function_registry().get_function_names()) {
                std::cout << " - " << name << "\n";
            }
            return 0;
        }
        if (flag == "--out-functions") {
            auto metadata = system.function_registry().get_function_metadata();
            for (size_t i = 0; i < metadata.size(); ++i) {
                std::cout << metadata[i] << (i < metadata.size() - 1 ? "|" : "");
            }
            return 0;
        }
        if (flag == "--list-constants") {
            std::cout << "Built-in Constants:\n";
            for (std::string c : system.constant_registry().get_constant_names())
            {
                std::cout << " - " << c << "\n";
            }
            return 0;
        }
        if (flag == "--out-constants") {
            // Check for -v next
            std::string next_arg;
            for (int j = i + 1; j < argc; j++)
            {
                next_arg = argv[j];
                verbose = verbose | (next_arg == "-v");
            }

            // Build n print
            std::vector<std::string> const_names = system.constant_registry().get_constant_names();
            std::vector<std::string> const_descriptions = system.constant_registry().get_constant_descriptions();
            if (const_names.size() > 0) 
            {
                std::cout << const_names.front() << (verbose ? ":" + const_descriptions.front() : "");
                const_names.erase(const_names.begin());
                const_descriptions.erase(const_descriptions.begin());
            }
            for (size_t i = 0; i < const_names.size(); i++)
            {
                std::cout << "|" << const_names.at(i) << (verbose ? ":" + const_descriptions.at(i) : "");
            }
            return 0;
        }

        // Not specified: path
        if (flag[0] != '-') input_path = flag;
    }

    if (input_path.empty()) {
        std::cerr << "Error: No input file specified." << std::endl;
        print_help();
        return 1;
    }

    try {
        // Execution Pipeline
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

        // Output
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
                    if (v.is_reserved) continue;
                    
                    *out << std::left << std::setw(20) << v.name 
                         << std::setw(15) << std::fixed << std::setprecision(6) << v.value 
                         << std::setw(9) << (v.unit_name.empty() ? "-" : "[" + v.unit_name + "]")
                         << (v.is_fixed ? " Fixed" : "Solved") << "\n";
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
