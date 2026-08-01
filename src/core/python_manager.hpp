#ifndef CONES_CORE_PYTHON_MANAGER_HPP
#define CONES_CORE_PYTHON_MANAGER_HPP

#include "platform.hpp"
#include <iostream>
#include <string>
#include <filesystem>
#include <cstdlib>
#include <vector>
#include <cstdio>

namespace cones {

/**
 * @brief Manages the Python environment for CoNES Studio and tools.
 * Handles virtual environment creation and package installation.
 */
class PythonManager {
    std::filesystem::path root_path_;
    std::filesystem::path venv_path_;
    std::string base_interp_;
    bool is_windows_;

public:
    explicit PythonManager(std::filesystem::path exe_path, std::string base_interp = "python3") 
        : base_interp_(std::move(base_interp)) {
        
        // Resolve the true executable path to ensure .venv is in the project root
        // even if cnes.exe is called from PATH.
        std::filesystem::path actual_path = get_executable_path();
        if (actual_path.empty()) {
            actual_path = std::filesystem::absolute(exe_path);
        }
        
        root_path_ = actual_path.parent_path();
        venv_path_ = root_path_ / ".venv";

#ifdef _WIN32
        is_windows_ = true;
#else
        is_windows_ = false;
#endif
    }

    std::filesystem::path get_interpreter() {
        std::filesystem::path venv_interp = venv_path_ / (is_windows_ ? "Scripts\\python.exe" : "bin/python3");
        if (std::filesystem::exists(venv_interp)) {
            return venv_interp;
        }
        return base_interp_;
    }

    bool ensure_venv() {
        if (std::filesystem::exists(venv_path_)) return true;

        std::cout << ">>> CoNES: Validating Python environment (" << base_interp_ << ")..." << std::endl;
        
        // Pre-flight check for tkinter - ensure base_interp is quoted
        std::string check_cmd = "\"" + base_interp_ + "\" -c \"import tkinter; import _tkinter\" > NUL 2>&1";
        if (!is_windows_) check_cmd = "\"" + base_interp_ + "\" -c \"import tkinter; import _tkinter\" > /dev/null 2>&1";
        
        if (std::system(wrap_command(check_cmd).c_str()) != 0) {
            std::cerr << ">>> CoNES Error: The selected Python interpreter does not have Tkinter installed." << std::endl;
            std::cerr << "    - If using Homebrew: brew install python-tk" << std::endl;
            std::cerr << "    - If on Linux: sudo apt install python3-tk" << std::endl;
            return false;
        }

        std::cout << ">>> CoNES: Creating virtual environment at " << venv_path_ << "..." << std::endl;
        
        std::string cmd = "\"" + base_interp_ + "\" -m venv \"" + venv_path_.string() + "\"";
        int result = std::system(wrap_command(cmd).c_str());

        if (result != 0) {
            std::cerr << ">>> CoNES Error: Failed to create virtual environment. Ensure python3 is installed and in your PATH." << std::endl;
            return false;
        }

        std::cout << ">>> CoNES: Virtual environment created successfully." << std::endl;
        return true;
    }

    std::filesystem::path find_package_path() {
        std::filesystem::path interpreter = get_interpreter();
        std::string interp_str = interpreter.string();
        
        // Command to find the package directory:
        // <python> -c "import os, cones; print(os.path.dirname(cones.__file__))"
        std::string redirect = is_windows_ ? " 2> NUL" : " 2> /dev/null";
        std::string cmd = "\"" + interp_str + "\" -c \"import os, cones; print(os.path.dirname(cones.__file__))\"" + redirect;
        
        std::string result = "";
#ifdef _WIN32
        // On Windows, wrap in cmd /c to handle quotes correctly
        std::string win_cmd = "cmd.exe /c " + wrap_command(cmd);
        FILE* pipe = _popen(win_cmd.c_str(), "r");
#else
        FILE* pipe = popen(cmd.c_str(), "r");
#endif
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                result += buffer;
            }
#ifdef _WIN32
            _pclose(pipe);
#else
            pclose(pipe);
#endif
        }
        
        // Trim whitespace and newlines
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ' || result.back() == '\t')) {
            result.pop_back();
        }
        
        if (!result.empty() && std::filesystem::exists(result)) {
            return std::filesystem::path(result);
        }
        return "";
    }

    int run_script(const std::string& script_rel_path, const std::vector<std::string>& args = {}) {
        std::filesystem::path script_path = (root_path_ / std::filesystem::path(script_rel_path)).lexically_normal();
        if (!std::filesystem::exists(script_path)) {
            // Script not found relative to executable path. Try finding it inside the installed cones package!
            std::filesystem::path pkg_path = find_package_path();
            if (!pkg_path.empty()) {
                script_path = (pkg_path / std::filesystem::path(script_rel_path)).lexically_normal();
            }
        }

        if (!std::filesystem::exists(script_path)) {
            std::cerr << ">>> CoNES Error: Script not found at " << script_path << std::endl;
            return 1;
        }

        if (!ensure_venv()) return 1;

        std::string interp = get_interpreter().string();
        std::string cmd = "\"" + interp + "\" \"" + script_path.string() + "\"";
        for (const auto& arg : args) {
            cmd += " \"" + arg + "\"";
        }

        std::cerr << ">>> CoNES: Starting IDE (" << cmd << ")" << std::endl;
        return std::system(wrap_command(cmd).c_str());
    }
};

} // namespace cones

#endif
