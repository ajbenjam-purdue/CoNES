#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include "core/version.hpp"
#include "core/system.hpp"
#include "lang/token.hpp"
#include "lang/lexer.hpp"
#include "lang/parser.hpp"

namespace nb = nanobind;

// Following nanobind.readthedocs.io
NB_MODULE(cones, m) {
    m.doc() = "CoNES Python Bindings";

    // Static binds for methods (string, full) and read-only vals (MAJ/MIN/PAT)
    nb::class_<cones::Version>(m, "Version")
        .def_static("string", &cones::Version::string)
        .def_static("full", &cones::Version::full)
        .def_ro_static("MAJOR", &cones::Version::MAJOR)
        .def_ro_static("MINOR", &cones::Version::MINOR)
        .def_ro_static("PATCH", &cones::Version::PATCH);

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
        .def("get_equation_line", &cones::System::get_equation_line);

    // Parser
    nb::class_<cones::Parser>(m, "Parser")
        .def("__init__", [](cones::Parser* p, std::vector<cones::Token> tokens, cones::System& sys, std::string path, int depth) {
            new (p) cones::Parser(std::move(tokens), sys, std::filesystem::path(path), depth);
        }, nb::arg("tokens"), nb::arg("sys"), nb::arg("initial_path") = ".", nb::arg("depth") = 0)
        .def("set_exe_path", [](cones::Parser& p, std::string path) { p.set_exe_path(std::filesystem::path(path)); })
        .def("parse", &cones::Parser::parse);
}
