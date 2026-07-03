#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/eigen/dense.h>
#include <nanobind/operators.h>
#include <sstream>

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
#include "core/property_types.hpp"
#include "core/dual_number.hpp"
#include "core/substance.hpp"
#include "core/ideal_gas.hpp"
#include "core/tabulated_substance.hpp"
#include "core/unit_registry.hpp"

namespace nb = nanobind;

// Following nanobind.readthedocs.io
NB_MODULE(_cones, m) {
    m.doc() = "CoNES Python Bindings";

    m.def("register_builtin_functions", &cones::register_builtin_functions, nb::arg("reg"), nb::arg("manager"));

    // Static binds for methods (string, full) and read-only vals (MAJ/MIN/PAT)
    nb::class_<cones::Version>(m, "Version")
        .def_static("string", &cones::Version::string)
        .def_static("full", &cones::Version::full)
        .def_ro_static("MAJOR", &cones::Version::MAJOR)
        .def_ro_static("MINOR", &cones::Version::MINOR)
        .def_ro_static("PATCH", &cones::Version::PATCH);

    // PropertyType
    nb::enum_<cones::PropertyType>(m, "PropertyType")
        .value("TEMPERATURE", cones::PropertyType::TEMPERATURE)
        .value("PRESSURE", cones::PropertyType::PRESSURE)
        .value("ENTHALPY", cones::PropertyType::ENTHALPY)
        .value("ENTROPY", cones::PropertyType::ENTROPY)
        .value("INTERNAL_ENERGY", cones::PropertyType::INTERNAL_ENERGY)
        .value("SPECIFIC_VOLUME", cones::PropertyType::SPECIFIC_VOLUME)
        .value("DENSITY", cones::PropertyType::DENSITY)
        .value("QUALITY", cones::PropertyType::QUALITY)
        .value("VISCOSITY", cones::PropertyType::VISCOSITY)
        .value("CONDUCTIVITY", cones::PropertyType::CONDUCTIVITY)
        .value("PRANDTL", cones::PropertyType::PRANDTL)
        .value("REYNOLDS", cones::PropertyType::REYNOLDS)
        .value("SATURATION_PRESSURE", cones::PropertyType::SATURATION_PRESSURE)
        .value("SATURATION_TEMPERATURE", cones::PropertyType::SATURATION_TEMPERATURE)
        .value("H_F", cones::PropertyType::H_F)
        .value("H_G", cones::PropertyType::H_G)
        .value("S_F", cones::PropertyType::S_F)
        .value("S_G", cones::PropertyType::S_G)
        .value("T_PH", cones::PropertyType::T_PH)
        .value("T_PS", cones::PropertyType::T_PS)
        .value("UNKNOWN", cones::PropertyType::UNKNOWN)
        .export_values();

    m.def("string_to_property", &cones::string_to_property, nb::arg("s"));
    m.def("property_to_string", &cones::property_to_string, nb::arg("p"));

    // DualNumber
    nb::class_<cones::DualNumber>(m, "DualNumber")
        .def(nb::init<double, double>(), nb::arg("val") = 0.0, nb::arg("der") = 0.0)
        .def_rw("val", &cones::DualNumber::val)
        .def_rw("der", &cones::DualNumber::der)
        .def(nb::self + nb::self)
        .def(nb::self - nb::self)
        .def(nb::self * nb::self)
        .def(nb::self / nb::self)
        .def(nb::self + double())
        .def(nb::self - double())
        .def(nb::self * double())
        .def(nb::self / double())
        .def(double() + nb::self)
        .def(double() - nb::self)
        .def(double() * nb::self)
        .def(double() / nb::self)
        .def(-nb::self)
        .def("pow", &cones::DualNumber::pow, nb::arg("p"))
        .def("__repr__", [](const cones::DualNumber &d) {
            return "DualNumber(val=" + std::to_string(d.val) + ", der=" + std::to_string(d.der) + ")";
        });

    // DualRow
    nb::class_<cones::DualRow>(m, "DualRow")
        .def(nb::init<double>(), nb::arg("val") = 0.0)
        .def(nb::init<double, int>(), nb::arg("val"), nb::arg("size"))
        .def(nb::init<double, Eigen::VectorXd>(), nb::arg("val"), nb::arg("der"))
        .def_rw("val", &cones::DualRow::val)
        .def_rw("der", &cones::DualRow::der)
        .def(nb::self + nb::self)
        .def(nb::self - nb::self)
        .def(nb::self * nb::self)
        .def(nb::self / nb::self)
        .def(nb::self + double())
        .def(nb::self - double())
        .def(nb::self * double())
        .def(nb::self / double())
        .def(double() + nb::self)
        .def(double() - nb::self)
        .def(double() * nb::self)
        .def(double() / nb::self)
        .def(-nb::self)
        .def("__repr__", [](const cones::DualRow &d) {
            std::stringstream ss;
            ss << "DualRow(val=" << d.val << ", der=[";
            for (int i = 0; i < d.der.size(); ++i) {
                ss << d.der(i) << (i == d.der.size() - 1 ? "" : ", ");
            }
            ss << "])";
            return ss.str();
        });

    // DualNumber math functions
    m.def("sin", nb::overload_cast<const cones::DualNumber&>(&cones::sin), nb::arg("d"));
    m.def("cos", nb::overload_cast<const cones::DualNumber&>(&cones::cos), nb::arg("d"));
    m.def("tan", nb::overload_cast<const cones::DualNumber&>(&cones::tan), nb::arg("d"));
    m.def("asin", nb::overload_cast<const cones::DualNumber&>(&cones::asin), nb::arg("d"));
    m.def("acos", nb::overload_cast<const cones::DualNumber&>(&cones::acos), nb::arg("d"));
    m.def("atan", nb::overload_cast<const cones::DualNumber&>(&cones::atan), nb::arg("d"));
    m.def("sinh", nb::overload_cast<const cones::DualNumber&>(&cones::sinh), nb::arg("d"));
    m.def("cosh", nb::overload_cast<const cones::DualNumber&>(&cones::cosh), nb::arg("d"));
    m.def("tanh", nb::overload_cast<const cones::DualNumber&>(&cones::tanh), nb::arg("d"));
    m.def("exp", nb::overload_cast<const cones::DualNumber&>(&cones::exp), nb::arg("d"));
    m.def("log", nb::overload_cast<const cones::DualNumber&>(&cones::log), nb::arg("d"));
    m.def("log10", nb::overload_cast<const cones::DualNumber&>(&cones::log10), nb::arg("d"));
    m.def("sqrt", nb::overload_cast<const cones::DualNumber&>(&cones::sqrt), nb::arg("d"));
    m.def("abs", nb::overload_cast<const cones::DualNumber&>(&cones::abs), nb::arg("d"));
    m.def("pow", nb::overload_cast<const cones::DualNumber&, double>(&cones::pow), nb::arg("d"), nb::arg("p"));

    // DualRow math functions
    m.def("sin", nb::overload_cast<const cones::DualRow&>(&cones::sin), nb::arg("d"));
    m.def("cos", nb::overload_cast<const cones::DualRow&>(&cones::cos), nb::arg("d"));
    m.def("tan", nb::overload_cast<const cones::DualRow&>(&cones::tan), nb::arg("d"));
    m.def("asin", nb::overload_cast<const cones::DualRow&>(&cones::asin), nb::arg("d"));
    m.def("acos", nb::overload_cast<const cones::DualRow&>(&cones::acos), nb::arg("d"));
    m.def("atan", nb::overload_cast<const cones::DualRow&>(&cones::atan), nb::arg("d"));
    m.def("sinh", nb::overload_cast<const cones::DualRow&>(&cones::sinh), nb::arg("d"));
    m.def("cosh", nb::overload_cast<const cones::DualRow&>(&cones::cosh), nb::arg("d"));
    m.def("tanh", nb::overload_cast<const cones::DualRow&>(&cones::tanh), nb::arg("d"));
    m.def("exp", nb::overload_cast<const cones::DualRow&>(&cones::exp), nb::arg("d"));
    m.def("log", nb::overload_cast<const cones::DualRow&>(&cones::log), nb::arg("d"));
    m.def("log10", nb::overload_cast<const cones::DualRow&>(&cones::log10), nb::arg("d"));
    m.def("sqrt", nb::overload_cast<const cones::DualRow&>(&cones::sqrt), nb::arg("d"));
    m.def("abs", nb::overload_cast<const cones::DualRow&>(&cones::abs), nb::arg("d"));
    m.def("pow", nb::overload_cast<const cones::DualRow&, double>(&cones::pow), nb::arg("d"), nb::arg("p"));

    // PropertyArg and PropertyArgRow
    nb::class_<cones::PropertyArg>(m, "PropertyArg")
        .def(nb::init<cones::PropertyType, cones::DualNumber>(), nb::arg("type"), nb::arg("value"))
        .def_rw("type", &cones::PropertyArg::type)
        .def_rw("value", &cones::PropertyArg::value);

    nb::class_<cones::PropertyArgRow>(m, "PropertyArgRow")
        .def(nb::init<cones::PropertyType, cones::DualRow>(), nb::arg("type"), nb::arg("value"))
        .def_rw("type", &cones::PropertyArgRow::type)
        .def_rw("value", &cones::PropertyArgRow::value);

    // Unit
    nb::class_<cones::Unit>(m, "Unit")
        .def(nb::init<>())
        .def(nb::init<double, std::vector<int>, double>(), nb::arg("scale"), nb::arg("dims"), nb::arg("offset") = 0.0)
        .def_rw("scale", &cones::Unit::scale)
        .def_rw("offset", &cones::Unit::offset)
        .def_rw("dims", &cones::Unit::dims)
        .def("to_string", &cones::Unit::to_string)
        .def("is_dimensionless", &cones::Unit::is_dimensionless)
        .def("requires_positivity", &cones::Unit::requires_positivity)
        .def("compatible", &cones::Unit::compatible, nb::arg("other"))
        .def("to_si", &cones::Unit::to_si)
        .def("pow", &cones::Unit::pow, nb::arg("p"))
        .def(nb::self * nb::self)
        .def(nb::self * double())
        .def(nb::self / nb::self)
        .def_static("Dimensionless", &cones::Unit::Dimensionless)
        .def_static("Meter", &cones::Unit::Meter)
        .def_static("Second", &cones::Unit::Second)
        .def_static("Kilogram", &cones::Unit::Kilogram)
        .def_static("Kelvin", &cones::Unit::Kelvin)
        .def_static("Celsius", &cones::Unit::Celsius)
        .def_static("Newton", &cones::Unit::Newton)
        .def_static("Joule", &cones::Unit::Joule)
        .def_static("Pascal", &cones::Unit::Pascal)
        .def_static("Watt", &cones::Unit::Watt)
        .def_static("Mol", &cones::Unit::Mol)
        .def("__repr__", [](const cones::Unit& u) {
            return "Unit(scale=" + std::to_string(u.scale) + 
                   ", dims=[" + std::to_string(u.dims[0]) + "," + std::to_string(u.dims[1]) + "," + std::to_string(u.dims[2]) + "," + std::to_string(u.dims[3]) + "," + std::to_string(u.dims[4]) + "]" +
                   ", offset=" + std::to_string(u.offset) + ")";
        });

    // UnitDefinition
    nb::class_<cones::UnitDefinition>(m, "UnitDefinition")
        .def_ro("name", &cones::UnitDefinition::name)
        .def_ro("unit", &cones::UnitDefinition::unit)
        .def_ro("description", &cones::UnitDefinition::description);

    // UnitRegistry
    nb::class_<cones::UnitRegistry>(m, "UnitRegistry")
        .def(nb::init<>())
        .def("register_unit", &cones::UnitRegistry::register_unit, nb::arg("name"), nb::arg("unit"), nb::arg("desc") = "")
        .def("get", &cones::UnitRegistry::get, nb::rv_policy::reference_internal, nb::arg("name"))
        .def("get_all_names", &cones::UnitRegistry::get_all_names);

    // Substance, IdealGasSubstance, TabulatedSubstance
    nb::class_<cones::Substance>(m, "Substance")
        .def("name", &cones::Substance::name)
        .def("summary", &cones::Substance::summary)
        .def("evaluate", nb::overload_cast<cones::PropertyType, const std::vector<cones::PropertyArg>&>(&cones::Substance::evaluate, nb::const_), nb::arg("target"), nb::arg("inputs"))
        .def("evaluate", nb::overload_cast<cones::PropertyType, const std::vector<cones::PropertyArgRow>&>(&cones::Substance::evaluate, nb::const_), nb::arg("target"), nb::arg("inputs"));

    nb::class_<cones::IdealGasSubstance, cones::Substance>(m, "IdealGasSubstance")
        .def(nb::init<std::string, double, double>(), nb::arg("name"), nb::arg("R"), nb::arg("Cp"));

    nb::class_<cones::TabulatedSubstance, cones::Substance>(m, "TabulatedSubstance")
        .def(nb::init<std::string>(), nb::arg("name"))
        .def("load_table", &cones::TabulatedSubstance::load_table, nb::arg("prop"), nb::arg("path"));

    // FuncArg and FuncArgRow
    nb::class_<cones::FuncArg>(m, "FuncArg")
        .def(nb::init<std::string, cones::DualNumber, cones::Unit>(), nb::arg("name"), nb::arg("value"), nb::arg("unit"))
        .def_rw("name", &cones::FuncArg::name)
        .def_rw("value", &cones::FuncArg::value)
        .def_rw("unit", &cones::FuncArg::unit);

    nb::class_<cones::FuncArgRow>(m, "FuncArgRow")
        .def(nb::init<std::string, cones::DualRow, cones::Unit>(), nb::arg("name"), nb::arg("value"), nb::arg("unit"))
        .def_rw("name", &cones::FuncArgRow::name)
        .def_rw("value", &cones::FuncArgRow::value)
        .def_rw("unit", &cones::FuncArgRow::unit);

    // IFunction
    nb::class_<cones::IFunction>(m, "IFunction")
        .def("name", &cones::IFunction::name)
        .def("args_metadata", &cones::IFunction::args_metadata)
        .def("description", &cones::IFunction::description)
        .def("validate", &cones::IFunction::validate, nb::arg("args"))
        .def("validate_row", &cones::IFunction::validate_row, nb::arg("args"))
        .def("evaluate", &cones::IFunction::evaluate, nb::arg("args"))
        .def("evaluate_row", &cones::IFunction::evaluate_row, nb::arg("args"))
        .def("get_unit", &cones::IFunction::get_unit, nb::arg("input_units"));

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
        .def(nb::init<>())
        .def("size", &cones::VariableRegistry::size)
        .def("register_variable", &cones::VariableRegistry::register_variable, nb::arg("name"), nb::arg("line") = -1)
        .def("set_value", &cones::VariableRegistry::set_value, nb::arg("index"), nb::arg("val"), nb::arg("is_si") = false)
        .def("set_lower_bound", &cones::VariableRegistry::set_lower_bound, nb::arg("index"), nb::arg("val"), nb::arg("is_si") = false)
        .def("set_upper_bound", &cones::VariableRegistry::set_upper_bound, nb::arg("index"), nb::arg("val"), nb::arg("is_si") = false)
        .def("set_bounds", &cones::VariableRegistry::set_bounds, nb::arg("index"), nb::arg("lower"), nb::arg("upper"), nb::arg("is_si") = false)
        .def("set_fixed", &cones::VariableRegistry::set_fixed, nb::arg("index"), nb::arg("fixed"))
        .def("set_reserved", &cones::VariableRegistry::set_reserved, nb::arg("index"), nb::arg("reserved"))
        .def("set_unit", &cones::VariableRegistry::set_unit, nb::arg("index"), nb::arg("unit"), nb::arg("name") = "")
        .def("suggest_guess", &cones::VariableRegistry::suggest_guess, nb::arg("index"), nb::arg("unit"))
        .def("get_index", &cones::VariableRegistry::get_index, nb::arg("name"))
        .def("get_variable", nb::overload_cast<int>(&cones::VariableRegistry::get_variable, nb::const_), nb::rv_policy::reference_internal, nb::arg("index"))
        .def("get_variable", nb::overload_cast<const std::string&>(&cones::VariableRegistry::get_variable, nb::const_), nb::rv_policy::reference_internal, nb::arg("name"))
        .def("get_active_indices", &cones::VariableRegistry::get_active_indices)
        .def("get_active_values", &cones::VariableRegistry::get_active_values)
        .def("update_active_values", &cones::VariableRegistry::update_active_values, nb::arg("x_active"))
        .def("apply_bounds", &cones::VariableRegistry::apply_bounds);

    // Constant
    nb::class_<cones::Constant>(m, "Constant")
        .def_ro("name", &cones::Constant::name)
        .def_ro("value", &cones::Constant::value)
        .def_ro("unit", &cones::Constant::unit)
        .def_ro("desc", &cones::Constant::desc);

    // ConstantRegistry
    nb::class_<cones::ConstantRegistry>(m, "ConstantRegistry")
        .def(nb::init<>())
        .def("register_constant", &cones::ConstantRegistry::register_constant, nb::arg("name"), nb::arg("value"), nb::arg("unit") = "", nb::arg("desc") = "")
        .def("get", &cones::ConstantRegistry::get, nb::rv_policy::reference_internal, nb::arg("name"))
        .def("get_constant_names", &cones::ConstantRegistry::get_constant_names)
        .def("get_constant_descriptions", &cones::ConstantRegistry::get_constant_descriptions)
        .def("load_standard_constants", &cones::ConstantRegistry::load_standard_constants);

    // FunctionRegistry
    nb::class_<cones::FunctionRegistry>(m, "FunctionRegistry")
        .def(nb::init<>())
        .def("register_function", &cones::FunctionRegistry::register_function, nb::arg("func"))
        .def("get", &cones::FunctionRegistry::get, nb::arg("name"))
        .def("get_function_names", &cones::FunctionRegistry::get_function_names)
        .def("get_function_metadata", &cones::FunctionRegistry::get_function_metadata);

    // SubstanceManager
    nb::class_<cones::SubstanceManager>(m, "SubstanceManager")
        .def(nb::init<>())
        .def("register_substance", &cones::SubstanceManager::register_substance, nb::arg("sub"))
        .def("register_ideal_gasses", &cones::SubstanceManager::register_ideal_gasses)
        .def("get", &cones::SubstanceManager::get, nb::arg("name"))
        .def("get_substance_names", &cones::SubstanceManager::get_substance_names)
        .def("load_materials", &cones::SubstanceManager::load_materials, nb::arg("directory_path"));

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

    // Lexer function
    nb::class_<cones::Lexer>(m, "Lexer")
        .def(nb::init<std::string>())
        .def("scan_tokens", &cones::Lexer::scan_tokens);

    // RoutineDef
    nb::class_<cones::RoutineDef>(m, "RoutineDef")
        .def_ro("name", &cones::RoutineDef::name)
        .def_ro("params", &cones::RoutineDef::params);

    // FunctionDef
    nb::class_<cones::FunctionDef>(m, "FunctionDef")
        .def_ro("name", &cones::FunctionDef::name)
        .def_ro("params", &cones::FunctionDef::params)
        .def_ro("return_unit", &cones::FunctionDef::return_unit)
        .def_ro("return_unit_name", &cones::FunctionDef::return_unit_name);

    // DefinitionRegistry
    nb::class_<cones::DefinitionRegistry>(m, "DefinitionRegistry")
        .def("get_routine", &cones::DefinitionRegistry::get_routine, nb::rv_policy::reference_internal, nb::arg("name"))
        .def("get_function", &cones::DefinitionRegistry::get_function, nb::rv_policy::reference_internal, nb::arg("name"))
        .def("get_routine_names", &cones::DefinitionRegistry::get_routine_names)
        .def("get_function_names", &cones::DefinitionRegistry::get_function_names);

    // System
    nb::class_<cones::System>(m, "System")
        .def(nb::init<>())
        .def("get_equation_count", &cones::System::get_equation_count)
        .def("get_equation_plaintext", &cones::System::get_equation_plaintext, nb::arg("index"))
        .def("get_equation_line", &cones::System::get_equation_line, nb::arg("index"))
        .def("registry", nb::overload_cast<>(&cones::System::registry), nb::rv_policy::reference_internal)
        .def("constant_registry", nb::overload_cast<>(&cones::System::constant_registry), nb::rv_policy::reference_internal)
        .def("function_registry", nb::overload_cast<>(&cones::System::function_registry), nb::rv_policy::reference_internal)
        .def("substance_manager", nb::overload_cast<>(&cones::System::substance_manager), nb::rv_policy::reference_internal)
        .def("unit_registry", nb::overload_cast<>(&cones::System::unit_registry), nb::rv_policy::reference_internal)
        .def("definition_registry", nb::overload_cast<>(&cones::System::definition_registry), nb::rv_policy::reference_internal)
        .def("evaluate", [](const cones::System& sys) {
            Eigen::VectorXd f;
            Eigen::MatrixXd j;
            sys.evaluate(f, j);
            return std::make_pair(f, j);
        });

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
        .def("set_blocking", &cones::NewtonSolver::set_blocking, nb::arg("blocking"))
        .def("solve", &cones::NewtonSolver::solve, nb::arg("system"));
}
