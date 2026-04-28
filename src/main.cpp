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

    std::cout << Version::full() << " (Coupled Nonlinear Equation Solver)\n"
              << "Usage: cnes [input.cnes] [options]\n\n"
              << "Options:\n\n"
              << "  Output:\n"
              << "    -o <file>             Write results to a file\n"
              << "    -v                    Verbose output (solver iterations)\n"
              << "    -s, --silent          Suppress the execution summary table\n"
              << "    --json                Output results in JSON format\n\n"
              << "  Solver:\n"
              << "    --tol <val>           Override convergence tolerance (default: 1e-9)\n"
              << "    --max-iter <val>      Override max solver iterations (default: 100)\n\n"
              << "  Development Tools:\n"
              << "    --lint                Check syntax and definitions without solving\n"
              << "    --list-substances     List all registered substances\n"
              << "    --list-functions      List all registered functions\n"
              << "    --list-constants      List all built-in constants\n"
              << "    --out-vscode-metadata Export all substances, functions, and constants lists as separated by |||, including rich data for each\n\n"
              << "  General:\n"
              << "    --version             Show the current CoNES version\n"
              << "    --help, -h            Show this help message\n" << std::endl;
}

void print_json_output(const System& system, double t_lexer, double t_parser, double t_solver, bool success, const std::string& error_msg = "") {
    std::cout << "{\n";
    std::cout << "  \"version\": \"" << Version::full() << "\",\n";
    std::cout << "  \"success\": " << (success ? "true" : "false") << ",\n";
    if (!error_msg.empty()) {
        std::cout << "  \"error\": \"" << error_msg << "\",\n";
    }
    std::cout << "  \"variables\": [\n";
    
    auto& reg = system.registry();
    bool first = true;
    for (size_t i = 0; i < reg.size(); ++i) {
        const auto& v = reg.get_variable(i);
        if (v.is_reserved) continue;
        
        if (!first) std::cout << ",\n";
        first = false;

        double display_val = (v.value / v.unit.scale) - v.unit.offset;
        
        std::cout << "    {\n";
        std::cout << "      \"name\": \"" << v.name << "\",\n";
        std::cout << "      \"value\": " << std::fixed << std::setprecision(10) << display_val << ",\n";
        std::cout << "      \"unit\": \"" << (v.unit_name.empty() ? "" : v.unit_name) << "\",\n";
        std::cout << "      \"is_fixed\": " << (v.is_fixed ? "true" : "false") << "\n";
        std::cout << "    }";
    }
    std::cout << "\n  ],\n";
    std::cout << "  \"performance\": {\n";
    std::cout << "    \"lexer_ms\": " << t_lexer << ",\n";
    std::cout << "    \"parser_ms\": " << t_parser << ",\n";
    std::cout << "    \"solver_ms\": " << t_solver << "\n";
    std::cout << "  }\n";
    std::cout << "}\n";
}

int main(int argc, char* argv[]) {

    // Get executable directory for portable material loading
    std::filesystem::path exe_path = std::filesystem::absolute(argv[0]).parent_path();
    std::filesystem::path materials_path = exe_path / "materials";

    // Init System & Default Environment
    System system;
    system.constant_registry().load_standard_constants();

    // Pull in ideal gases (src: engineeringtoolbox.com)
    auto air = std::make_shared<IdealGasSubstance>("Air", 287.05, 1005.0);
    auto argon = std::make_shared<IdealGasSubstance>("Argon", 208.1, 520.0);
    auto carbon_dioxide = std::make_shared<IdealGasSubstance>("CO2", 188.9, 845.9);
    auto nitrogen = std::make_shared<IdealGasSubstance>("Nitrogen", 296.8, 1041.0);
    auto oxygen = std::make_shared<IdealGasSubstance>("O2", 259.8, 918.9);
    
    // Register them
    system.substance_manager().register_substance(air);
    system.substance_manager().register_substance(argon);
    system.substance_manager().register_substance(carbon_dioxide);
    system.substance_manager().register_substance(nitrogen);
    system.substance_manager().register_substance(oxygen);

    // Automatically load Tabulated Substances from /materials relative to exe
    if (std::filesystem::exists(materials_path)) {
        for (const auto& entry : std::filesystem::directory_iterator(materials_path)) {
            if (entry.path().extension() == ".cnesbin") {
                std::string fname = entry.path().stem().string(); // e.g., "Water_h"
                size_t underscore_pos = fname.find('_');
                if (underscore_pos == std::string::npos) continue;

                std::string sub_name = fname.substr(0, underscore_pos);
                std::string prop_code = fname.substr(underscore_pos + 1);
                
                // Special handling for multi-word property codes (T_ph, etc.)
                PropertyType prop = string_to_property(prop_code);
                if (prop == PropertyType::UNKNOWN) {
                    // Try T_ph / T_ps specifically if string_to_property is strict
                    if (prop_code == "T_ph") prop = PropertyType::T_PH;
                    else if (prop_code == "T_ps") prop = PropertyType::T_PS;
                }
                
                if (prop == PropertyType::UNKNOWN) continue;

                auto sub = std::dynamic_pointer_cast<TabulatedSubstance>(system.substance_manager().get(sub_name));
                if (!sub) {
                    sub = std::make_shared<TabulatedSubstance>(sub_name);
                    system.substance_manager().register_substance(sub);
                }
                sub->load_table(prop, entry.path().string());
            }
        }
    }

    // Register Built-in Functions (Math & Property)
    register_builtin_functions(system.function_registry(), system.substance_manager());

    // Argument Parsing
    std::string input_path = "";
    std::string output_path = "";
    bool verbose = false;
    bool silent = false;
    bool json_out = false;
    bool lint_mode = false;
    double tol_override = 1e-9;
    int max_iter_override = 100;

    for (int i = 1; i < argc; ++i) {
        std::string flag = argv[i];
        if (flag == "--help" || flag == "-h") { print_help(); return 0; }
        if (flag == "--version") { std::cout << Version::full() << std::endl; return 0; }
        if (flag == "-v") { verbose = true; continue; }
        if (flag == "-s" || flag == "--silent") { silent = true; continue; }
        if (flag == "--json") { json_out = true; continue; }
        if (flag == "--lint") { lint_mode = true; continue; }
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
        if (flag == "--list-functions") {
            std::cout << "Registered Functions:\n";
            for (const auto& name : system.function_registry().get_function_names()) {
                std::cout << " - " << name << "\n";
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
        if (flag == "--out-vscode-metadata") {
            // CONSTANTS_STR ||| FUNCTIONS_STR ||| SUBSTANCES_STR
            
            // Constants: Name:Value:Unit:Description
            std::vector<std::string> c_names = system.constant_registry().get_constant_names();
            for (size_t i = 0; i < c_names.size(); ++i) {
                const auto* c = system.constant_registry().get(c_names[i]);
                std::cout << c->name << ":" << c->value << ":" << c->unit << ":" << c->desc << (i < c_names.size() - 1 ? "|" : "");
            }
            std::cout << "|||";

            // Functions: Sig:Desc
            auto f_meta = system.function_registry().get_function_metadata();
            for (size_t i = 0; i < f_meta.size(); ++i) {
                std::cout << f_meta[i] << (i < f_meta.size() - 1 ? "|" : "");
            }
            std::cout << "|||";

            // Substances: Name:Summary
            auto s_names = system.substance_manager().get_substance_names();
            for (size_t i = 0; i < s_names.size(); ++i) {
                auto sub = system.substance_manager().get(s_names[i]);
                std::cout << sub->name() << ":" << sub->summary() << (i < s_names.size() - 1 ? "|" : "");
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
        Parser parser(lexer.scan_tokens(), system, input_path);
        parser.set_exe_path(exe_path);
        auto time_lexer = std::chrono::high_resolution_clock::now();

        parser.parse();
        auto time_parser = std::chrono::high_resolution_clock::now();

        if (lint_mode) {
            if (json_out) {
                print_json_output(system, 
                    std::chrono::duration<double, std::milli>(time_lexer - start_time).count(),
                    std::chrono::duration<double, std::milli>(time_parser - time_lexer).count(),
                    0, true);
            } else {
                std::cout << "Linting successful: " << input_path << std::endl;
            }
            return 0;
        }

        NewtonSolver solver(tol_override, max_iter_override, verbose);
        solver.solve(system);

        auto end_time = std::chrono::high_resolution_clock::now();

        double d_lexer = std::chrono::duration<double, std::milli>(time_lexer - start_time).count();
        double d_parser = std::chrono::duration<double, std::milli>(time_parser - time_lexer).count();
        double d_solver = std::chrono::duration<double, std::milli>(end_time - time_parser).count();

        if (json_out) {
            print_json_output(system, d_lexer, d_parser, d_solver, true);
            return 0;
        }

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
                    
                    double display_val = (v.value / v.unit.scale) - v.unit.offset;

                    *out << std::left << std::setw(20) << v.name 
                         << std::setw(15) << std::fixed << std::setprecision(6) << display_val 
                         << std::setw(9) << (v.unit_name.empty() ? "-" : "[" + v.unit_name + "]")
                         << (v.is_fixed ? " Fixed" : "Solved") << "\n";
                }

                *out << std::string(50, '=') << "\n";
                *out << "Lexer Time:  " << d_lexer << " ms\n";
                *out << "Parser Time: " << d_parser << " ms\n";
                *out << "Solver Time: " << d_solver << " ms\nDOF: " << system.registry().get_active_indices().size() << "\n";
                *out << std::string(50, '=') << std::endl;
            }
        }

    } catch (const std::bad_alloc&) {
        if (json_out) {
            print_json_output(system, 0, 0, 0, false, "System error: Out of memory. The script may be too large or contains circular dependencies.");
        } else {
            std::cerr << "\nFATAL ERROR: System out of memory." << std::endl;
        }
        return 1;
    } catch (const std::exception& e) {
        if (json_out) {
            print_json_output(system, 0, 0, 0, false, e.what());
        } else {
            std::cerr << "\nFATAL ERROR: " << e.what() << std::endl;
        }
        return 1;
    }

    return 0;
}
