#ifndef CONES_CORE_PYTHON_MANAGER_HPP
#define CONES_CORE_PYTHON_MANAGER_HPP

#include <iostream>
#include <string>
#include <filesystem>
#include <cstdlib>
#include <vector>

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
        root_path_ = std::filesystem::absolute(exe_path).parent_path();
        venv_path_ = root_path_ / ".venv";
#ifdef _WIN32
        is_windows_ = true;
#else
        is_windows_ = false;
#endif
    }

    std::filesystem::path get_interpreter() {
        std::filesystem::path venv_interp = venv_path_ / (is_windows_ ? "Scripts/python.exe" : "bin/python3");
        if (std::filesystem::exists(venv_interp)) {
            return venv_interp;
        }
        return base_interp_;
    }

    bool ensure_venv() {
        if (std::filesystem::exists(venv_path_)) return true;

        std::cout << ">>> CoNES: Validating Python environment (" << base_interp_ << ")..." << std::endl;
        
        // Pre-flight check for tkinter
        std::string check_cmd = base_interp_ + " -c \"import tkinter; import _tkinter\" > NUL 2>&1";
        if (!is_windows_) check_cmd = base_interp_ + " -c \"import tkinter; import _tkinter\" > /dev/null 2>&1";
        
        if (std::system(check_cmd.c_str()) != 0) {
            std::cerr << ">>> CoNES Error: The selected Python interpreter does not have Tkinter installed." << std::endl;
            std::cerr << "    - If using Homebrew: brew install python-tk" << std::endl;
            std::cerr << "    - If on Linux: sudo apt install python3-tk" << std::endl;
            return false;
        }

        std::cout << ">>> CoNES: Creating virtual environment at " << venv_path_ << "..." << std::endl;
        
        std::string cmd = base_interp_ + " -m venv \"" + venv_path_.string() + "\"";
        int result = std::system(cmd.c_str());

        if (result != 0) {
            std::cerr << ">>> CoNES Error: Failed to create virtual environment. Ensure python3 is installed and in your PATH." << std::endl;
            return false;
        }

        std::cout << ">>> CoNES: Virtual environment created successfully." << std::endl;
        return true;
    }

    int run_script(const std::string& script_rel_path, const std::vector<std::string>& args = {}) {
        if (!ensure_venv()) return 1;

        std::filesystem::path script_path = root_path_ / script_rel_path;
        if (!std::filesystem::exists(script_path)) {
            std::cerr << ">>> CoNES Error: Script not found at " << script_path << std::endl;
            return 1;
        }

        std::string interp = get_interpreter().string();
        std::string cmd = "\"" + interp + "\" \"" + script_path.string() + "\"";
        for (const auto& arg : args) {
            cmd += " \"" + arg + "\"";
        }

        return std::system(cmd.c_str());
    }
};

} // namespace cones

#endif
