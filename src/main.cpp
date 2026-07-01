#include "lang/lexer.hpp"
#include "lang/parser.hpp"
#include "solver/newton_solver.hpp"
#include "core/property_functions.hpp"
#include "core/tabulated_substance.hpp"
#include "core/version.hpp"
#include "core/python_manager.hpp"
#include "core/platform.hpp"
#include <iostream>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <chrono>
#include <filesystem>

// Platform agnostics
#ifdef _WIN32
bool win = true;
#else
bool win = false;
#endif

using namespace cones;

// Helper structure for table printing
struct dataColumn
{
    std::string name;
    int column_width = 0;
    std::vector<std::string> data;

    dataColumn(std::string data_name) : name(data_name), column_width(static_cast<int>(data_name.size())) {}

    void add_data(int val)
    {
        std::string new_val = std::to_string(val);
        data.emplace_back(new_val);
        column_width = std::max(column_width, static_cast<int>(new_val.size()));
    }
    void add_data(double val)
    {
        std::string new_val = std::format("{:.5g}", val);
        data.emplace_back(new_val);
        column_width = std::max(column_width, static_cast<int>(new_val.size()));
    }
    void add_data() // Empty
    {
        data.emplace_back("-");
        column_width = std::max(column_width, 1);
    }
    void add_data(std::string val) // Empty
    {
        data.emplace_back(val);
        column_width = std::max(column_width, static_cast<int>(val.size()));
    }
};

struct TimeContainer
{
    double t_lexer = 0, t_parser = 0, t_solver = 0;
};

// True if the file exists and is of the correct ending
bool is_cnes(std::string path)
{
    return std::filesystem::exists(path) && path.ends_with(".cnes");
}

// Print the help output message.
void print_help()
{
    std::cout << Version::full() << " (Coupled Nonlinear Equation Solver)\n"
              << "" << Version::lang_full() << "\n"
              << "Usage: cnes [input_file.cnes] [options]\n\n"
              << "Options:\n\n"
              << "  Output Parameters:\n"
              << "    -o <file>             Write results to a file\n"
              << "    -v                    Verbose output (solver iterations)\n"
              << "    -s, --silent          Suppress the execution summary table\n"
              << "    -j, --json            Output results in JSON format\n\n"
              << "  Solver Parameters:\n"
              << "    --tol <val>           Override convergence tolerance (default: 1e-9)\n"
              << "    --max-iter <val>      Override max solver iterations (default: 100)\n\n"
              << "  Development Tools:\n"
              << "    -L, --lint            Check syntax and definitions of the input file without solving\n"
              << "    --list-substances     (Standalone) List all registered substances\n"
              << "    --list-functions      (Standalone) List all registered functions\n"
              << "    --list-constants      (Standalone) List all built-in constants\n"
              << "    --out-vscode-metadata (Standalone) Export all substances, functions, and constants lists as separated by |||, including rich data for each\n\n"
              << "  General/Other:\n"
              << "    --version             Show the current CoNES version\n"
              << "    --help, -h            Show this help message\n"
              << "    --build-substances    (Standalone) Attempt to build the binary substance tables using Python\n"
              << "    --IDE [int. path]     (Standalone) Open the Python-based IDE (Need python on PATH, or optionally provide the desired interpreter's path)\n"
              << std::endl;
}

// Print the raw json string including metadata, variables, and solution metrics like solve time
void print_json_output(const System &system, const SolverReport &report, const TimeContainer &timeContainer)
{
    // Timing
    const auto& [t_lexer, t_parser, t_solver] = timeContainer;

    // Metadata
    std::cout << "{\n";
    std::cout << "  \"version\": \"" << Version::full() << "\",\n";
    std::cout << "  \"success\": " << (report.success ? "true" : "false") << ",\n";

    // An error exists, provide it
    if (!report.error_msg.empty())
    {
        std::cout << "  \"error\": " << std::quoted(report.error_msg) << ",\n";
    }

    // Print all the variables
    std::cout << "  \"variables\": [\n";

    auto &reg = system.registry();
    bool first = true;
    for (size_t i = 0; i < reg.size(); ++i)
    {
        // Get only non-kwd variables
        const auto &v = reg.get_variable(i);
        if (v.is_reserved)
            continue;

        if (!first) std::cout << ",\n";
        first = false;
        double display_val = (v.value / v.unit.scale) - v.unit.offset;
        std::cout << "    {\n";
        std::cout << "      \"name\": " << std::quoted(v.name) << ",\n";
        std::cout << "      \"value\": " << std::fixed << std::setprecision(10) << display_val << ",\n";
        std::cout << "      \"unit\": " << std::quoted(v.unit_name.empty() ? "" : v.unit_name) << ",\n";
        std::cout << "      \"line\": " << v.line << ",\n";
        std::cout << "      \"is_fixed\": " << (v.is_fixed ? "true" : "false") << "\n";
        std::cout << "    }";
    }
    std::cout << "\n  ],\n";

    // Residuals
    std::cout << "  \"residuals\": [\n";
    double residuals_total = 0;
    for (size_t i = 0; i < system.get_equation_count(); ++i)
    {
        if (i > 0) std::cout << ",\n";
        double res_val = 0.0;
        if (report.residuals.size() > (int)i) {
            res_val = report.residuals(i);
            residuals_total += std::abs(res_val);
        }
        std::cout << "    {\n";
        std::cout << "      \"id\": " << i << ",\n";
        std::cout << "      \"expression\": " << std::quoted(system.get_equation_plaintext(i)) << ",\n";
        std::cout << "      \"value\": " << std::scientific << std::setprecision(10) << res_val << ",\n";
        std::cout << "      \"line\": " << system.get_equation_line(i) << "\n";
        std::cout << "    }";
    }
    std::cout << "\n  ],\n";

    // Solution metrics
    std::cout << "  \"performance\": {\n";
    std::cout << "    \"lexer_ms\": " << t_lexer << ",\n";
    std::cout << "    \"parser_ms\": " << t_parser << ",\n";
    std::cout << "    \"solver_ms\": " << t_solver << ",\n";
    std::cout << "    \"total_residuals\": " << residuals_total << ",\n";
    std::cout << "    \"iterations\": " << report.iterations << "\n";
    std::cout << "  }\n";
    std::cout << "}\n";
}

// Print a table using the abode dataColumn struct
void print_table(std::ostream *out, std::string title, std::vector<dataColumn> data)
{
    // Set column widths
    std::vector<int> column_widths(data.size());
    size_t total_width = 0;
    size_t max_row = 0;
    for (size_t i = 0; i < data.size(); i++)
    {
        int x = data.at(i).column_width;
        column_widths.at(i) = x;
        total_width += x + 1;
        max_row = std::max(max_row, data.at(i).data.size());
    }
    total_width = std::max(total_width, title.size()+1);

    // Header & Cols
    *out << "\n";
    *out << std::string(total_width, '=') << "\n";
    *out << " " << title << "\n";
    *out << std::string(total_width, '=') << "\n";
    for (dataColumn& col : data)
    {
        *out << std::left << std::setw(col.column_width) << col.name << " ";
    }
    *out << "\n";
    *out << std::string(total_width, '-') << "\n";

    // Data
    for (size_t row = 0; row < max_row; row++)
    {
        for (dataColumn& col : data)
        {
            *out << std::left << std::setw(col.column_width) << col.data.at(row) << " ";
        }
        *out << "\n";
    }
    *out << std::string(total_width, '=') << "\n";
}

void list_constants(System& system)
{
    std::cout << "Built-in Constants:\n";
    for (std::string c : system.constant_registry().get_constant_names())
    {
        std::cout << " - " << c << "\n";
    }
}

void list_functions(System& system)
{
    std::cout << "Registered Functions:\n";
    for (const auto &name : system.function_registry().get_function_names())
    {
        std::cout << " - " << name << "\n";
    }
}

void list_substances(System& system)
{
    std::cout << "Registered Substances:\n";
    for (const auto &name : system.substance_manager().get_substance_names())
    {
        std::cout << " - " << name << "\n";
    }
}

void print_metadata(System system)
{
    // Constants: Name:Value:Unit:Description
    std::vector<std::string> c_names = system.constant_registry().get_constant_names();
    for (size_t i = 0; i < c_names.size(); ++i)
    {
        const auto *c = system.constant_registry().get(c_names[i]);
        std::cout << c->name << ":" << c->value << ":" << c->unit << ":" << c->desc << (i < c_names.size() - 1 ? "|" : "");
    }
    std::cout << "|||";

    // Functions: Sig:Desc
    auto f_meta = system.function_registry().get_function_metadata();
    for (size_t i = 0; i < f_meta.size(); ++i)
    {
        std::cout << f_meta[i] << (i < f_meta.size() - 1 ? "|" : "");
    }
    std::cout << "|||";

    // Substances: Name:Summary
    auto s_names = system.substance_manager().get_substance_names();
    for (size_t i = 0; i < s_names.size(); ++i)
    {
        auto sub = system.substance_manager().get(s_names[i]);
        std::cout << sub->name() << ":" << sub->summary() << (i < s_names.size() - 1 ? "|" : "");
    }
}

void lint_out(bool lint_mode, bool json_out, const System& system, const TimeContainer& performance)
{
    if (lint_mode)
    {
        if (json_out)
        {
            SolverReport report;
            report.success = true;
            print_json_output(system, report, performance);
        }
        else
        {
            std::cout << "Linting successful." << std::endl;
        }
    }
}

// Build the substances library using the Python interpreter on PATH
int build_substances(const std::filesystem::path& exe_path)
{
    PythonManager py(exe_path);
    return py.run_script("tools/export_props.py");
}

// Open CoNES studio via the Python interpreter on PATH
int open_ide(const std::filesystem::path& exe_path, std::string py_interp_path = "python3", std::string cnes_file_path = "")
{
    PythonManager py(exe_path, py_interp_path);
    std::cout << ">>> CoNES: Launching CoNES Studio..." << std::endl;
    
    std::vector<std::string> args;
    if (!cnes_file_path.empty()) args.push_back(cnes_file_path);
    
    return py.run_script("cones_studio/main.py", args);
}

int main(int argc, char *argv[])
{

    // Get executable directory for portable material loading
    std::filesystem::path actual_exe_path = get_executable_path();
    if (actual_exe_path.empty()) actual_exe_path = std::filesystem::absolute(argv[0]);
    
    std::filesystem::path exe_path = actual_exe_path.parent_path();
    std::filesystem::path materials_path = exe_path / "materials";

    // Init System & Default Environment
    System system;
    system.constant_registry().load_standard_constants(); // TODO: Make API more uniform with location of constants/functions/substances
    system.substance_manager().register_ideal_gasses(); // Ideal gas definitions are in the substance manager

    // Automatically load Tabulated Substances from /materials relative to exe OR load automatically on launch
    if (!std::filesystem::exists(materials_path))
    {
        // Try finding it inside the installed cones python package
        PythonManager py(actual_exe_path);
        std::filesystem::path pkg_path = py.find_package_path();
        if (!pkg_path.empty()) {
            std::filesystem::path pkg_materials = pkg_path / "materials";
            if (std::filesystem::exists(pkg_materials)) {
                materials_path = pkg_materials;
            }
        }
    }

    if (std::filesystem::exists(materials_path))
    {
        system.substance_manager().load_materials(materials_path.string());
    }
    else
    {
        build_substances(actual_exe_path);
    }

    // Register Built-in Functions (Math & Property)
    register_builtin_functions(system.function_registry(), system.substance_manager());

    // Argument Parsing
    std::string input_path(""), output_path("");
    bool verbose(false), silent(false), json_out(false), lint_mode(false);
    double tol_override = 1e-9;
    int max_iter_override = 500;

    // Flags
    for (int i = 1; i < argc; ++i)
    {
        std::string flag = argv[i];

        // Print the help message
        if (flag == "--help" || flag == "-h")
        {
            print_help();
            return 0;
        }

        // Print the version
        if (flag == "--version")
        {
            std::cout << Version::full() << " / " << Version::lang_full() << "\nCopyright (c) 2026 ajbenjam-purdue.\nThis is free and open-sourced software available under an MIT license.\nThere is NO warranty." << std::endl;
            return 0;
        }

        // (Re)Build the substance library manually
        if (flag == "--build-substances")
        {
            build_substances(actual_exe_path);
            return 0;
        }

        // Open the IDE
        if (flag == "--IDE")
        {
            std::string cnes_file_path(""), py_interpreter_path("python3"), arg("");
            while (i < argc - 1)
            {
                arg = argv[++i];
                if (is_cnes(arg))
                {
                    cnes_file_path = arg;
                }
                else 
                {
                    py_interpreter_path = arg;
                }
            }
            open_ide(actual_exe_path, py_interpreter_path, cnes_file_path);
            return 0;
        }

        // Verbose execution
        if (flag == "-v" && !silent)
        {
            verbose = true;
            continue;
        }

        // Silent execution
        if ((flag == "-s" || flag == "--silent")  && !verbose)
        {
            silent = true;
            continue;
        }

        // JSON output
        if (flag == "-j" || flag == "--json")
        {
            json_out = true;
            continue;
        }

        // Lint mode (stops before solving)
        if (flag == "-L" || flag == "--lint")
        {
            lint_mode = true;
            continue;
        }

        // Set output path
        if (flag == "-o" && i + 1 < argc)
        {
            output_path = argv[++i];
            continue;
        }

        // Set the solver tolerance
        if (flag == "--tol" && i + 1 < argc)
        {
            tol_override = std::stod(argv[++i]);
            continue;
        }

        // Set the solver maximum iterations
        if (flag == "--max-iter" && i + 1 < argc)
        {
            max_iter_override = std::stoi(argv[++i]);
            continue;
        }

        // List the available substances
        if (flag == "--list-substances")
        {
            list_substances(system);
            return 0;
        }

        // List the available functions
        if (flag == "--list-functions")
        {
            list_functions(system);
            return 0;
        }

        // List the available constants
        if (flag == "--list-constants")
        {
            list_constants(system);
            return 0;
        }

        // (DEPRECATED) Yield the long metadata string containing all the metadata necessary to update the vscode extension
        if (flag == "--out-vscode-metadata")
        {
            print_metadata(system);
            return 0;
        }

        // Not specified: path
        if (flag[0] != '-')
            input_path = flag;
    }

    if (input_path.empty())
    {
        std::cerr << "Error: No input file specified." << std::endl;
        print_help();
        return 1;
    }

    // Execution Pipeline
    // Attempt to open file, catch a likely memalloc error
    try
    {
        std::ifstream file(input_path);
        if (!file.is_open())
            throw std::runtime_error("Could not open file: " + input_path);
        std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        // Initialize lexer & parser
        auto start_time = std::chrono::high_resolution_clock::now();
        Lexer lexer(source);
        Parser parser(lexer.scan_tokens(), system, input_path); // Tokenize the text and provide to the parser
        parser.set_exe_path(exe_path);                          //
        auto time_lexer = std::chrono::high_resolution_clock::now();

        parser.parse(); // Analyze the tokens
        auto time_parser = std::chrono::high_resolution_clock::now();

        // Package timing data
        double d_lexer = std::chrono::duration<double, std::milli>(time_lexer - start_time).count();
        double d_parser = std::chrono::duration<double, std::milli>(time_parser - time_lexer).count();
        TimeContainer performance(d_lexer, d_parser, 0);

        // For strict LSP mode, just output the results so far
        lint_out(lint_mode, json_out, system, performance);
        if (lint_mode) return 0;

        // Otherwise, solve system and yield the results
        NewtonSolver solver(tol_override, max_iter_override, verbose);
        SolverReport report = solver.solve(system);

        // Get total duration and yield the duration
        auto end_time = std::chrono::high_resolution_clock::now(); // Total time & durations
        double d_solver = std::chrono::duration<double, std::milli>(end_time - time_parser).count();
        performance.t_solver = d_solver;

        // For json output, yield the results via json
        if (json_out)
        {
            print_json_output(system, report, performance);
            return 0;
        }

        // Non-json output
        if (!silent || !output_path.empty())
        {
            if (!report.success) {
                std::cerr << "\nWARNING: Solver failed to converge! Results below are non-physical." << std::endl;
                std::cerr << "Error: " << report.error_msg << "\n" << std::endl;
            }
            std::ostream *out = &std::cout;
            std::ofstream file_out;

            // A file exists
            if (!output_path.empty())
            {
                file_out.open(output_path);
                out = &file_out;
            }

            // We want to see the table
            if (!silent)
            {
                // Create and populate the columns
                dataColumn data_variable("Variable"), data_value("Value"), data_unit("Unit"), data_state("State");
                auto &reg = system.registry();
                for (size_t i = 0; i < reg.size(); ++i) // Loop the registry to add data
                {
                    const auto &v = reg.get_variable(i);
                    if (v.is_reserved) // Ignore reserved (kwd)
                        continue;
                    data_variable.add_data(v.name);
                    data_value.add_data((v.value / v.unit.scale) - v.unit.offset);
                    data_unit.add_data((v.unit_name.empty() ? "-" : "[" + v.unit_name + "]"));
                    data_state.add_data((v.is_fixed ? " Fixed" : "Solved"));
                }

                // Built the vector and print the table
                std::vector<dataColumn> data = {data_variable, data_value, data_unit, data_state}; // Variable, value, unit, state
                print_table(&std::cout, Version::full() + " Execution Summary", data);
            }
        }
    }
    catch (const std::bad_alloc &)
    {
        if (json_out)
        {
            SolverReport report;
            report.success = false;
            report.error_msg = "System error: Out of memory. The script may be too large or contains circular dependencies.";
            print_json_output(system, report, TimeContainer());
        }
        else
        {
            std::cerr << "\nFATAL ERROR: System out of memory." << std::endl;
        }
        return 1;
    }
    catch (const std::exception &e)
    { // Blunder! This shouldn't be necessary anymore EXCEPT for file IO
        if (json_out)
        {
            SolverReport report;
            report.success = false;
            report.error_msg = e.what();
            print_json_output(system, report, TimeContainer());
        }
        else
        {
            std::cerr << "\nFATAL ERROR: " << e.what() << std::endl;
        }
        return 1;
    }

    return 0;
}
