# CoNES: Coupled Nonlinear Equation Solver

A high-performance C++ environment for solving large-scale systems of coupled nonlinear equations, specializing in thermophysical systems. CoNES utilizes a symbolic-numerical hybrid approach with a custom interpreted scripting language. For guidance on the design, usage, and best practices of the language, please see [the standards](CoNES-Standards.md). For information on previous builds, release candidates, and releases, see [the changelog](CHANGELOG.md). If you are looking to contribute to CoNES or build the project from source, please see [the contributor guide](CONTRIBUTING.md).

---

## 0. Compilation & Usage

### Precompiled Binaries

For quick setup without a C++ compiler, you can download precompiled standalone CLI executables (`cnes` / `cnes.exe`) for Windows, Linux, and macOS directly from the **GitHub Releases** page.

### Build Options & Purposes

CoNES supports four distinct build options depending on whether you are building the standalone C++ CLI, developing Python bindings, or generating release packages:

### Option 1: Standalone C++ CLI via CMake
* **Purpose**: Compiles only the C++ command-line tool (`cnes` / `cnes.exe`) without requiring Python or `nanobind`
* **Command**:
  ```bash
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCONES_BUILD_PYTHON=OFF
  cmake --build build --config Release --target cnes
  ```
* **Output**: Executable located at `build/cnes` (Linux/macOS) or `build/Release/cnes.exe` (Windows). *As a reminder, the binary will look for the `tools/` and `materials/` directories from its own directory*

### Option 2: Direct Compiler Invocation (Minimal CLI Build)
* **Purpose**: Single-command compile directly from source when CMake is not present (requires `Eigen/` in the project root)
* **Command**:
  * **macOS / Linux**: `g++ -O3 -std=c++20 -I . src/main.cpp -o cnes`
  * **Windows (MSVC)**: `cl /O2 /std:c++20 /I . src/main.cpp /Fe:cnes.exe`
  * **Windows (MinGW)**: `g++ -O3 -std=c++20 -I . src/main.cpp -o cnes.exe`

### Option 3: Python Package & Bindings Installation via `pip`
* **Purpose**: Compiles the C++ core via `scikit-build-core` and `nanobind` into a native Python module (`_cones`), installs the `cones` Python package (`import cones`), and installs the `cnes` CLI globally into your system `PATH`
* **Command**:
  * **Development / Editable Mode**: `pip install --editable .`
  * **Standard Installation**: `pip install .`

### Option 4: Packaging Wheels and Source Distributions
* **Purpose**: Generates distribution packages (binary wheels `.whl` and source archives `.tar.gz`) for GitHub Releases (maybe PyPI in the future, too)
* **Command**:
  ```bash
  pip install build
  python -m build
  ```
* **Output**: Package files generated in `dist/`

### Interpreter Usage

The binary acts as a lightweight virtual machine. It can be used to solve a cnes script and display the results, lint a cnes script file without solving, or simply provide access to view installed substances, functions, and constants. **Note!** The interpreter only has access to the correctly packaged binary substance tables available in `materaials/`. To build these tables, view section 6. Use `./cnes` on Unix or `cnes.exe` on Windows:

```bash
./cnes [input_file.cnes] [options]
```

### CLI Options
#### Generics
 - `--help`: Shows the help display with all supported flags
 - `--version`: Shows the current interpreter's version
 - `--IDE`: If it exists, opens the python IDE
 - `--build-substances`: If it exists, uses the python tool and CoolProp to create/update the `materials/` directory
 #### Solver parameters
 - `-o <file>`: Write results to a specific text file
 - `-v`: Verbose output (shows Newton-Raphson residuals per iteration)
 - `-s`, `--silent`: Suppress the execution summary table (useful for batch processing)
 - `-j`, `--json`: Output results as a JSON object (ideal for GUI/tool integration)
 - `-L`, `--lint`: Performs lexical and syntactic analysis and variable registration, but stops before solving
 - `--tol <val>`: Override the convergence tolerance (default: 1e-9)
 - `--max-iter <val>`: Override the maximum number of solver iterations (default: 100)
 #### Solver information
 - `--list-substances`: Display all registered materials (Ideal Gas and Tabulated)
 - `--list-functions`: Display all available math and property functions
 - `--list-constants`: Display all built-in constants
 - `--out-vscode-metadata`: Exports project metadata for the VS Code extension

## 1. Development Roadmap

 - **[DONE]** Modular Property Function Registry
 - **[DONE]** Inverted $T(P,h)$ and $T(P,s)$ lookups
 - **[DONE]** Heuristic initial guessing based on units
 - **[DONE]** Macro-style `routine` blocks for reusable physics and procedural `function` blocks with local scoping
 - **[DONE]** Recursive inclusion with robust path resolution
 - **[DONE]** Exposure of c++ methods and structures to Python
 - **[DONE]** Complete final SI unit (Coulomb)
 - **[DONE]** BLT Decomposition (Tarjan's SCC) for block-solving
 - **[DONE]** Improved parsing comprehension to limit divide-by-zero situations
 - **[DONE]** Temperature Glide Support for Zeotropic Mixtures
 - **[TODO]** IDE rework with nanobind module for improved performance
 - **[TODO]** VSCode Extension (proper) with integration for the releases workflow
 - **[TODO]** Bipartite Matching for DOF validation
 - **[TODO]** Psychrometrics!

## 2. System of Equations Solver (Core Engine)

The core engine transforms high-level mathematical relations into a solvable numerical system

### Symbolic Representation
 - **Expression Trees**: Equations are stored as Abstract Syntax Trees (AST), which facilitates symbolic differentiation and optimization prior to numerical execution
 - **Variable Registry**: A centralized manager mapping variable names to indices and managing the unit system, allowing for automatic and inferred conversions
 - **Automatic Differentiation (AD)**: Forward-mode differentiation using Dual Numbers provides derivatives where possible to accelerate solution cadence
 
### Structural Optimization
 - **Dependency Analysis**: The solver generates an adjacency matrix of the system
 - **BLT Decomposition**: Decomposes large systems into independent sub-blocks via strongly-connected components
 - **Incidence Checking**: Automatic detection of under-determined or over-determined systems via bipartite matching

### Numerical Execution
 - **Solver Loop**: [Newton-Raphson](https://en.wikipedia.org/wiki/Newton%27s_method) iteration with backtracking line search
   - This solver approach is (I believe) used in EES to find a solution; I find EES's implementation to need too much babysitting to avoid numerical instability, so my application of the same method features many strategies to counter this, listed below
 - **Robustness Features**:
   - **Variable Bounding**: Hard-coded physical limits accelerate convergence and reduce the likelihood of a divergent state; this associates many variables' limits automatically (i.e. $[T], [P] \ge 0$) while unbounded or unassociated units remain unconstrained
   - **Heuristic Guessing**: Automatically suggests ballpark initial guesses based on assigned units (e.g., 101 kPa for Pressure) to avoid singularities like division by zero
   - **Re-trial Attempts**: Unlike EES, this software will repeatedly re-attempt the computation in the event of a failure after scattering initial values; for a sufficiently tight solution tolerance, this may avoid numerical instabilities

## 3. Thermophysical Property System

CoNES features a modular, high-performance property engine designed for [EES](https://fchartsoftware.com/ees/)-like power and more modern and pythonic paradigms.

### Multi-Axis Tabulated Data
 - **Independent Axis Selection**: Properties are initially gridded on $P$ and $T$ for all current substances
 - **Inverted Lookups**: Supports direct lookups for $T(P, h)$ and $T(P, s)$ using pre-computed inverted grids, bypassing the need for nested iterations
 - **Saturation & Temperature Glide Support**: Automated redirection for saturation lookups and temperature glide. Two-phase lookups dynamically utilize bubble-point ($T_{\text{sat,f}}$) and dew-point ($T_{\text{sat,g}}$) saturation boundaries to compute phase-change temperatures for both pure fluids and zeotropic mixtures with glide: $T(P, X) = T_{\text{bubble}}(P) + X \cdot (T_{\text{dew}}(P) - T_{\text{bubble}}(P))$
 - **Two-Phase Properties**: High-speed calculation of two-phase enthalpy, entropy, etc., via $h_f + x(h_g - h_f)$ using saturated liquid/vapor boundary tables

### Material Support
 - **Ideal Gases**: Analytical models for Air and other ideal gases
 - **Tabulated Substances**: Gridded binary data (`.cnesbin`) for Water, R134a, R410A, R12, and more. See **CLI Options** for more information

## 4. CNES Script (Interpreter)

A domain-specific language designed for clear equation entry and property calls.

 - **Implicit Equations**: Supports `f(x) = g(x)` syntax
 - **Unit System**: Full support for SI and common engineering units ([degC], bar, kJ/kg, kW). Automatic conversion to internal SI representation (e.g. A temperature defined in `[degC]` or `[degF]` will be internally converted to and used as `[K]`)
 - **Inclusion System**: Robust modularity with `include`
   - **Search Path**: Searches (1) relative to the script, (2) Current Working Directory, and (3) `[exe]/libs/`. Automatically infers `.cnes` if omitted (e.g., `include "fluid_lib"`); this should be made more redundant or close in behavior to that defined in [PEP 302](https://peps.python.org/pep-0302/)
 - **Modularity**:
   - **Routines**: Macro-style equation templates that expand in the global solver scope
   - **User Functions**: Isolated procedural blocks with local variable scoping for sequential calculations
   - **Packaged Functions**: More rigid C++ functions which offer direct Dual Number support and improved performance at the cost of poor visibility
 - **VS Code Extension**: Native support with syntax highlighting and comment-aware autocomplete; This will be changing significantly in a future release

## 5. VS Code Extension

Install the packaged extension at `src/lang/cnes/cnes-0.1.1.vsix`:

```bash
code --install-extension src/lang/cnes/cnes-0.1.1.vsix
```
```bash
python3 tools/export_props.py
```

The `materials/` directory should begin to populate with a selection of `.cnesbin` binary substance tables, and the python script's output should confirm the successful creation of each one. Without creating these tables, CoNES will only have access to idea gases.

## 6. IDE

There is a lightweight and extremely simple Integrated Development Environment built using Python and tkinter/customtkinter. This can be found in `/cones_studio/main.py`, and is the easiest way to start writing with CoNES. The IDE can also be launched with the Binary, using `--IDE`.

## 7. Binary Table Creation

CoNES uses properties sourced from [CoolProp](https://coolprop.org), an open source database. To build the required binary tables to use fluids like `Water` or refrigerants like `R134a`, you must have CoolProp installed with `pip`. The creation of such tables will happen automatically and can be re-run using the binary (`cnes --build-substances`).

## 8. Python Bindings (nanobind)

CoNES provides high-performance native Python bindings compiled via [nanobind](https://github.com/wjakob/nanobind). This exposes the C++ Lexer, Parser, Solver, Units, Substance Manager, and Dual Number Automatic Differentiation engine directly to Python.

### Installation

```bash
# Standard package installation (includes global 'cnes' CLI)
pip install .

# Development / Editable mode
pip install --editable .
```

### Usage

Once installed, import `cones` in Python to programmatically parse, configure, and solve thermodynamic systems:

```python
import cones

system = cones.System()
system.constant_registry().load_standard_constants()
system.substance_manager().register_ideal_gasses()
cones.register_builtin_functions(system.function_registry(), system.substance_manager())

lexer = cones.Lexer("P := 101325 [Pa]\nT = 300 [K]\nv = SpecificVolume(Air, T=T, P=P)")
cones.Parser(lexer.scan_tokens(), system, ".").parse()

solver = cones.NewtonSolver(1e-9, 100, False)
solver.solve(system)
```

For a comprehensive walkthrough showcasing tabulated materials, temperature glide, and direct C++ property evaluation, see [`examples/demo.py`](examples/demo.py).
