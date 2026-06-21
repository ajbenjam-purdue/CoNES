#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/eigen/dense.h>
// #include <nanobind/stl/filesystem.h> // May not be available depending on nanobind version
#include "core/version.hpp"
#include "lang/token.hpp"
#include "lang/lexer.hpp"
#include "core/system.hpp"
#include "lang/parser.hpp"
#include "core/unit.hpp"
#include "core/variable_registry.hpp"
#include "core/constant_registry.hpp"
#include "core/function_registry.hpp"
#include "core/substance_manager.hpp"
#include "solver/newton_solver.hpp"
#include "core/property_functions.hpp"

namespace nb = nanobind;

// Following nanobind.readthedocs.io
NB_MODULE(cones, m) {
    m.doc() = "CoNES Python Bindings";

    m.def("register_builtin_functions", &cones::register_builtin_functions, nb::arg("reg"), nb::arg("manager"));

    // Static binds for methods (string, full) and read-only vals (MAJ/MIN/PAT)
    nb::class_<cones::Version>(m, "Version")
        .def_static("string", &cones::Version::string)
        .def_static("full", &cones::Version::full)
        .def_ro_static("MAJOR", &cones::Version::MAJOR)
        .def_ro_static("MINOR", &cones::Version::MINOR)
        .def_ro_static("PATCH", &cones::Version::PATCH);

    // Unit
    nb::class_<cones::Unit>(m, "Unit")
        .def(nb::init<>())
        .def("to_string", &cones::Unit::to_string);

    // Variable
    nb::class_<cones::Variable>(m, "Variable")
        .def_ro("name", &cones::Variable::name)
        .def_ro("index", &cones::Variable::index)
        .def_ro("value", &cones::Variable::value)
        .def_ro("lower_bound", &cones::Variable::lower_bound)
        .def_ro("upper_bound", &cones::Variable::upper_bound)
        .def_ro("is_fixed", &cones::Variable::is_fixed)
        .def_ro("is_reserved", &cones::Variable::is_reserved)
        .def_ro("unit_name", &cones::Variable::unit_name)
        .def_ro("line", &cones::Variable::line);

    // VariableRegistry
    nb::class_<cones::VariableRegistry>(m, "VariableRegistry")
        .def("size", &cones::VariableRegistry::size)
        .def("get_variable", nb::overload_cast<int>(&cones::VariableRegistry::get_variable, nb::const_), nb::rv_policy::reference_internal)
        .def("get_index", &cones::VariableRegistry::get_index);

    // Constant
    nb::class_<cones::Constant>(m, "Constant")
        .def_ro("name", &cones::Constant::name)
        .def_ro("value", &cones::Constant::value)
        .def_ro("unit", &cones::Constant::unit)
        .def_ro("desc", &cones::Constant::desc);

    // ConstantRegistry
    nb::class_<cones::ConstantRegistry>(m, "ConstantRegistry")
        .def("get", &cones::ConstantRegistry::get, nb::rv_policy::reference_internal)
        .def("get_constant_names", &cones::ConstantRegistry::get_constant_names)
        .def("get_constant_descriptions", &cones::ConstantRegistry::get_constant_descriptions)
        .def("load_standard_constants", &cones::ConstantRegistry::load_standard_constants);

    // FunctionRegistry
    nb::class_<cones::FunctionRegistry>(m, "FunctionRegistry")
        .def("get_function_names", &cones::FunctionRegistry::get_function_names)
        .def("get_function_metadata", &cones::FunctionRegistry::get_function_metadata);

    // SubstanceManager
    nb::class_<cones::SubstanceManager>(m, "SubstanceManager")
        .def("register_ideal_gasses", &cones::SubstanceManager::register_ideal_gasses)
        .def("get_substance_names", &cones::SubstanceManager::get_substance_names)
        .def("load_materials", &cones::SubstanceManager::load_materials);

    // All enum binds: Token Types
    nb::enum_<cones::TokenType>(m, "TokenType")
        .value("IDENTIFIER", cones::TokenType::IDENTIFIER)
        .value("NUMBER", cones::TokenType::NUMBER)
        .value("STRING", cones::TokenType::STRING)
        .value("PLUS", cones::TokenType::PLUS)
        .value("MINUS", cones::TokenType::MINUS)
        .value("STAR", cones::TokenType::STAR)
        .value("SLASH", cones::TokenType::SLASH)
        .value("CARET", cones::TokenType::CARET)
        .value("EQUALS", cones::TokenType::EQUALS)
        .value("COLON_EQUALS", cones::TokenType::COLON_EQUALS)
        .value("LPAREN", cones::TokenType::LPAREN)
        .value("RPAREN", cones::TokenType::RPAREN)
        .value("LBRACE", cones::TokenType::LBRACE)
        .value("RBRACE", cones::TokenType::RBRACE)
        .value("LBRACKET", cones::TokenType::LBRACKET)
        .value("RBRACKET", cones::TokenType::RBRACKET)
        .value("COLON", cones::TokenType::COLON)
        .value("COMMA", cones::TokenType::COMMA)
        .value("DOT", cones::TokenType::DOT)
        .value("UNDERSCORE", cones::TokenType::UNDERSCORE)
        .value("INCLUDE", cones::TokenType::INCLUDE)
        .value("ROUTINE", cones::TokenType::ROUTINE)
        .value("FUNCTION", cones::TokenType::FUNCTION)
        .value("RETURN", cones::TokenType::RETURN)
        .value("END", cones::TokenType::END)
        .value("END_OF_FILE", cones::TokenType::END_OF_FILE)
        .value("UNKNOWN", cones::TokenType::UNKNOWN)
        .export_values();

    // Tokenizer
    nb::class_<cones::Token>(m, "Token")
        .def(nb::init<cones::TokenType, std::string, int>())
        .def_ro("type", &cones::Token::type)
        .def_ro("lexeme", &cones::Token::lexeme)
        .def_ro("line", &cones::Token::line)
        .def("__repr__", [](const cones::Token& t) {
            return "<Token type=" + std::to_string(static_cast<int>(t.type)) + 
                   " lexeme='" + t.lexeme + "' line=" + std::to_string(t.line) + ">";
        });

    // Lexer functiuon
    nb::class_<cones::Lexer>(m, "Lexer")
        .def(nb::init<std::string>())
        .def("scan_tokens", &cones::Lexer::scan_tokens);

    // System
    nb::class_<cones::System>(m, "System")
        .def(nb::init<>())
        .def("get_equation_count", &cones::System::get_equation_count)
        .def("get_equation_plaintext", &cones::System::get_equation_plaintext)
        .def("get_equation_line", &cones::System::get_equation_line)
        .def("registry", nb::overload_cast<>(&cones::System::registry), nb::rv_policy::reference_internal)
        .def("constant_registry", nb::overload_cast<>(&cones::System::constant_registry), nb::rv_policy::reference_internal)
        .def("function_registry", nb::overload_cast<>(&cones::System::function_registry), nb::rv_policy::reference_internal)
        .def("substance_manager", nb::overload_cast<>(&cones::System::substance_manager), nb::rv_policy::reference_internal);

    // Parser
    nb::class_<cones::Parser>(m, "Parser")
        .def("__init__", [](cones::Parser* p, std::vector<cones::Token> tokens, cones::System& sys, std::string path, int depth) {
            new (p) cones::Parser(std::move(tokens), sys, std::filesystem::path(path), depth);
        }, nb::arg("tokens"), nb::arg("sys"), nb::arg("initial_path") = ".", nb::arg("depth") = 0)
        .def("set_exe_path", [](cones::Parser& p, std::string path) { p.set_exe_path(std::filesystem::path(path)); })
        .def("parse", &cones::Parser::parse);

    // SolverReport
    nb::class_<cones::SolverReport>(m, "SolverReport")
        .def_ro("success", &cones::SolverReport::success)
        .def_ro("error_msg", &cones::SolverReport::error_msg)
        .def_ro("iterations", &cones::SolverReport::iterations)
        .def_ro("residuals", &cones::SolverReport::residuals);

    // NewtonSolver
    nb::class_<cones::NewtonSolver>(m, "NewtonSolver")
        .def(nb::init<double, int, bool>(), nb::arg("tol") = 1e-9, nb::arg("max_iter") = 1000, nb::arg("verbose") = false)
        .def("solve", &cones::NewtonSolver::solve, nb::arg("system"));
}
