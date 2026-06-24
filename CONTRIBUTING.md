# Contributing to CoNES

This document is intended for developers looking to understand the internal architecture of CoNES, compile it from source, run tests, and contribute changes to the repository.

## 1. Project Directory Structure

Here is a high-level overview of the files and directories in the repository:

*   **`src/`**: The core C++ codebase
    *   `src/core/`: Contains the property systems (`Substance`, `TabulatedSubstance`), mathematical utilities (`DualNumber` for automatic differentiation), and platform configurations (`platform.hpp`)
    *   `src/lang/`: The custom scripting interpreter (Lexer, Parser, and AST nodes)
    *   `src/solver/`: The numerical solver implementation (Newton-Raphson with backtracking line search)
    *   `src/bindings.cpp`: Exposes C++ structures (Lexer, Parser, NewtonSolver, DualNumber, etc.) to Python via `nanobind`
    *   `src/main.cpp`: Entry point for the C++ standalone command-line interpreter (`cnes`)
*   **`cones/`**: The namespace package directory for Python. It contains `__init__.py` which initializes the `_cones` binary module and provides clean Pythonic imports
*   **`cones_studio/`**: A lightweight tkinter/customtkinter IDE for editing and running `.cnes` scripts
*   **`tools/`**: Python utility scripts (e.g., `export_props.py` which builds tabulated property tables from CoolProp)
*   **`materials/`**: The local directory containing prebuilt `.cnesbin` files (tabulated substances) used by the engine
*   **`tests/`**: Unit and integration tests for both Python bindings and C++ engines
*   **`CMakeLists.txt`**: Build configuration file for building both Python bindings and standalone executables
*   **`pyproject.toml`**: Packaging configuration utilizing `scikit-build-core` and `nanobind`
*   **`.github/workflows/wheels.yml`**: CI/CD automation workflow to build standalone CLI binaries and compile wheel distributions

## 2. Compilation Prerequisites & Architecture Targets

CoNES is designed to run on modern platforms. Due to dependencies on C++20 features (specifically `std::format` on floats/doubles and `std::to_chars` / `<charconv>`), CoNES targets the following compiler toolchains:

*   **Windows**: Windows 10/11 using MSVC (Visual Studio 2022+) or MinGW-w64 (GCC 13+)
*   **macOS**: macOS 13.3+ with AppleClang/Xcode 14.3+. Older SDK versions do not support floating-point formatting with `std::format` out-of-the-box
*   **Linux**: Modern distributions with GCC 13+ or Clang 16+

### Eigen Dependency
CoNES relies on the incredible Eigen library for matrix mathematics and linear solver operations
*   **Manual compilation**: Place the `Eigen` library directory directly in the project root
*   **CMake compilation**: If the `Eigen/` folder is not found in the repository root, CMake will automatically download Eigen 5.0.0 via `FetchContent` from GitLab

## 3. Building From Source

### Building the C++ Standalone CLI
To build only the C++ command-line tool `cnes` without Python bindings:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCONES_BUILD_PYTHON=OFF
cmake --build build --config Release --target cnes
```
This produces the executable in `build/cnes` (Linux/macOS) or `build/Release/cnes.exe` (Windows).

### Building Python Bindings and Package
CoNES uses `scikit-build-core` as the build backend, which wraps CMake and compiles the C++ codebase as a native Python extension

1.  **Prerequisites**:
    ```bash
    pip install scikit-build-core nanobind
    ```
2.  **Editable Mode (Development)**:
    For local development, install the package in editable mode so changes to the C++ bindings or Python wrappers take effect immediately upon recompilation:
    ```bash
    pip install --editable .
    ```
3.  **Regular Package Installation**:
    ```bash
    pip install .
    ```

### Packaging Wheels and Source Distributions
To build distribution packages (source distributions and binary wheels) to upload to GitHub Releases:
```bash
pip install build
python -m build
```
The output wheel (`.whl`) and source distribution (`.tar.gz`) files will be generated under the `dist/` directory

## 4. Run-Time Resource Resolution (`find_package_path`)

To ensure that the globally installed standalone `cnes` executable runs correctly without requiring a local repository checkout:
1.  On startup, `cnes` looks for the `materials/` folder in its current executable directory.
2.  If the folder is missing, it invokes the fallback function `find_package_path` inside [src/core/python_manager.hpp](src/core/python_manager.hpp).
3.  This calls Python internally (`import cones; os.path.dirname(cones.__file__)`) to fetch the installation directory of the `cones` Python package.
4.  It then uses that package directory to locate `materials/`, `tools/`, and `cones_studio/`.

## 5. Substance Database Table Creation

Tabulated fluids (such as `Water` or `R134a`) must be exported into `.cnesbin` files to be loaded by the engine. CoNES uses CoolProp to query and export these grids

To generate or rebuild the substance databases:
1.  Install CoolProp:
    ```bash
    pip install coolprop
    ```
2.  Execute the build command:
    ```bash
    cnes --build-substances
    ```
    This script runs the Python export tool ([tools/export_props.py](tools/export_props.py)) to populate the `materials/` directory

## 6. Running Tests

### C++ Tests
You can run compiled tests in the `tests/` directory:
- Linux/macOS: Run `/tests/test_lexer` or other compiled binaries
- Windows: Run `test_relationships.exe`, `test_lexer.exe`, etc

### Python Bindings Tests
To test the C++ bindings from Python:
1.  Make sure you have built the bindings locally (or installed in editable mode)
2.  Run the tests using standard `unittest`:
    ```bash
    python -m unittest tests/test_bindings.py
    ```

## 7. CI/CD Workflow (GitHub Actions)

The CI/CD pipeline is configured in [wheels.yml](.github/workflows/wheels.yml) and performs the following jobs when a new release is published:

1.  **build_wheels**: Builds binary wheels for Windows, Linux, and macOS platforms
    *   **Python Target Matrix**: Targets CPython 3.11 to 3.13. It skips older versions and 32-bit/musl architectures
    *   **macOS Toolchain**: Employs `MACOSX_DEPLOYMENT_TARGET=13.3` to ensure compilation compatibility with C++20 formatting features
    *   **Linux Toolchain**: Leverages `manylinux_2_28` and configures `gcc-toolset-13` as the build compiler to support `std::format`
2.  **build_sdist**: Generates Python source distributions
3.  **build_cli**: Compiles standalone CLI executables and archives them into platforms-specific zip/tar archives containing the executable and the `tools/` folder
4.  **release**: Gathers all wheels, source distributions, and CLI archives and uploads them to the GitHub release page

## 8. Development Guidelines

*   **Documentation Integrity**: Maintain existing comments and document newly introduced functions. Follow the documentation tagging style in [CoNES-Standards.md](CoNES-Standards.md) for public blocks
*   **Architecture Agnosticism**: Avoid introducing OS-specific dependencies outside of `src/core/platform.hpp` or conditional preprocessor blocks; abstract when possible