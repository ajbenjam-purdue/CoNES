# CoNES: Coupled Nonlinear Equation Solver

A high-performance C++ environment for solving large-scale systems of coupled nonlinear equations, specializing in thermophysical systems. CoNES utilizes a symbolic-numerical hybrid approach with a custom interpreted scripting language. For guidance on the design, usage, and best practices of the language, please see [the standards](CoNES-Standards.md). For information on previous builds, release candidates, and releases, see [the changelog](CHANGELOG.md).

---

## 0. Compilation & Usage

### Precompiled Binaries

For quick setup without a C++ compiler, you can download precompiled standalone CLI executables (`cnes` / `cnes.exe`) for Windows, Linux, and macOS directly from the **GitHub Releases** page.

### Manual Compilation

To compile the interpreter manually, ensure Eigen is in the project root and run:

**MacOS / Linux (g++/clang++):**
```bash
g++ -O3 -march=native -std=c++20 -I . src/main.cpp -o cnes
```
_`-march=native` allows g++/clang++ to make use of hardware features native to the building platform. If you're targeting an alternative build platform, disregard this or use `g++ -mcpu=help` to find all current targets._

_`-ffast-math` is an extremely effective optimization for math-heavy programs like CoNES, but it disallows the usage of infinity; CoNES may undergo some rework to allow this optimization, but does not currently support warn-free compilation with the flag._

**Windows (MSVC via Developer Command Prompt, untested):**
```cmd
cl /O2 /std:c++20 /I . src/main.cpp /Fe:cnes.exe
```

**Windows (g++ via MinGW-w64):**
```cmd
g++ -O3 -std=c++20 -I . src/main.cpp -o cnes.exe
```

### Interpreter Usage

The binary acts as a lightweight virtual machine. It can be used to solve a cnes script and display the results, lint a cnes script file without solving, or simply provide access to view installed substances, functions, and constants. **Note!** The interpreter only has access to the correctly packaged binary substance tables available in `materaials/`. To build these tables, view section 6. Use `./cnes` on Unix or `cnes.exe` on Windows:

```bash
./cnes [input_file.cnes] [options]
```

#### CLI Options
 - `-o <file>`: Write results to a specific text file.
 - `-v`: Verbose output (shows Newton-Raphson residuals per iteration).
 - `-s`, `--silent`: Suppress the execution summary table (useful for batch processing).
 - `-j`, `--json`: Output results as a JSON object (ideal for GUI/tool integration).
 - `-L`, `--lint`: Performs lexical and syntactic analysis and variable registration, but stops before solving. 
 - `--tol <val>`: Override the convergence tolerance (default: 1e-9).
 - `--max-iter <val>`: Override the maximum number of solver iterations (default: 100).
 - `--list-substances`: Display all registered materials (Ideal Gas and Tabulated).
 - `--list-functions`: Display all available math and property functions.
 - `--list-constants`: Display all built-in constants
 - `--out-vscode-metadata`: Exports project metadata for the VS Code extension.

---

## 1. System of Equations Solver (Core Engine)

The core engine transforms high-level mathematical relations into a solvable numerical problem.

### Symbolic Representation
 - **Expression Trees**: Equations are stored as Abstract Syntax Trees (AST). This facilitates symbolic differentiation and optimization before numerical execution.
 - **Variable Registry**: A centralized manager mapping variable names to indices. Internal operations use index-based access to `std::vector<double>` for $O(1)$ performance.
 - **Automatic Differentiation (AD)**: Forward-mode AD using Dual Numbers provides exact machine-precision derivatives without truncation errors or memory explosion.
 
### Structural Optimization
 - **Dependency Analysis**: The solver generates an adjacency matrix of the system.
 - **BLT Decomposition**: (Roadmap) Decomposes large systems into smaller, independent sub-blocks via Strongly Connected Components.
 - **Incidence Checking**: Automatic detection of under-determined or over-determined systems via bipartite matching.

### Numerical Execution
 - **Solver Loop**: [Newton-Raphson](https://en.wikipedia.org/wiki/Newton%27s_method) iteration with backtracking line search.
   - This solver approach is (I believe) used in EES to find a solution; I find EES's implementation to need too much babysitting to avoid numerical instability, so my application of the same method features many strategies to counter this:
 - **Robustness Features**:
   - **Variable Bounding**: Hard-coded physical limits prevent mathematical domain errors; this associates many variables' limits automatically (i.e. $[T], [P] \ge 0$) while unbounded or unassociated units remain unconstrained.
   - **Heuristic Guessing**: Automatically suggests ballpark initial guesses based on assigned units (e.g., 101 kPa for Pressure) to avoid singularities like division by zero. I'm not entirely happy with this as-is, and perhaps the software (likely in the studio script, not the actual VM) should iteratively learn.
   - **Re-trial Attempts**: Unlike EES, this software will repeatedly re-attempt the computation in the event of a failure after scattering initial values. This will not affect the final solved state of the system, but can potentially avoid numerical instability or singularities.

## 2. Thermophysical Property System

CoNES features a modular, high-performance property engine designed for [EES](https://fchartsoftware.com/ees/)-like power and more modern, often pythonic, principles.

### Multi-Axis Tabulated Data
 - **Independent Axis Selection**: Properties are gridded on the most stable axes ($P$ and $T$ for all current substances).
 - **Inverted Lookups**: Supports direct lookups for $T(P, h)$ and $T(P, s)$ using pre-computed inverted grids, bypassing the need for nested iterations.
 - **Saturation Support**: Automated redirection for saturation lookups. Using `Pressure(Water, T=T1, x=0.5)` automatically utilizes $P_{sat}(T)$ 1D tables. This critically does not account for Temperature glide in some instances and consequently needs future development.
 - **Two-Phase Properties**: High-speed calculation of two-phase enthalpy, entropy, etc., via $h_f + x(h_g - h_f)$ using saturated liquid/vapor boundary tables.

### Material Support
 - **Ideal Gases**: Analytical models for Air and other ideal gases.
 - **Tabulated Substances**: Gridded binary data (`.cnesbin`) for Water, R134a, R12, and more. See **CLI Options** for more information.

## 3. CNES Script (Interpreter)

A domain-specific language designed for clear equation entry and property calls.

 - **Implicit Equations**: Supports `f(x) = g(x)` syntax.
 - **Unit System**: Full support for SI and common engineering units (C, bar, kJ/kg, kW). Automatic conversion to internal SI representation (A temperature defined in $\degree{F}$ will be internally converted to and used as $\degree{C}$ but still display in $\degree{F}$).
 - **Inclusion System**: Robust modularity with `include`.
   - **Search Path**: Searches (1) relative to the script, (2) Current Working Directory, and (3) `[exe]/libs/`. Automatically infers `.cnes` if omitted (e.g., `include "fluid_lib"`).
 - **Modularity**:
   - **Routines**: Macro-style equation templates that expand in the global solver scope.
   - **Functions**: Isolated procedural blocks with local variable scoping for sequential calculations.
 - **VS Code Extension**: Native support with syntax highlighting and comment-aware autocomplete.

## 4. Development Roadmap

 - **[DONE]** Modular Property Function Registry.
 - **[DONE]** Inverted $T(P,h)$ and $T(P,s)$ lookups.
 - **[DONE]** Heuristic initial guessing based on units.
 - **[DONE]** Macro-style `routine` blocks for reusable physics.
 - **[DONE]** Procedural `function` blocks with local scoping.
 - **[DONE]** Recursive inclusion with robust path resolution.
 - **[DONE]** Exposure of c++ methods and structures to Python.
 - **[TODO]** BLT Decomposition (Tarjan's SCC) for block-solving.
 - **[TODO]** Improved parsing comprehension to limit divide-by-zero situations.
 - **[TODO]** Bipartite Matching for DOF validation.
 - **[TODO]** Psychrometrics!

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

CoNES can be compiled as a native Python extension using [nanobind](https://github.com/wjakob/nanobind). This allows you to directly access the C++ Lexer, Parser, Solver, Units, Substances, registries, and Automatic Differentiation structures from Python.

### Installation

#### Option 1: Install Precompiled Wheels (Recommended)
You can install precompiled wheels directly from the **GitHub Releases** page without needing a C++ compiler or CMake installed:
```bash
pip install <URL_TO_RELEASE_WHEEL_FILE>
```

#### Option 2: Compile and Install from Source
Ensure you have CMake (>=3.15) and a C++ compiler installed on your system, then run:
```bash
pip install .
```
This compiles and installs:
1. The **`cones`** Python library (for direct scripting in Python).
2. The **`cnes`** CLI executable tool (installed globally on your system `PATH` as a script/executable).

### Development & Contribution

If you are modifying the C++ source or Python bindings and want your changes to be active locally, install the package in editable mode:

```bash
pip install --editable .
```

To build distribution packages (source distributions and binary wheels) to upload to GitHub Releases:

1. Install `build`:
   ```bash
   pip install build
   ```
2. Build the distribution files:
   ```bash
   python -m build
   ```
   This compiles the project and generates package wheels (`.whl`) and source archives (`.tar.gz`) under the `dist/` directory.

### Usage

Once installed, the library can be imported from any Python script, and the CLI can be run globally via the shell command `cnes`.

#### Basic Workflow (Lexing, Parsing, and Solving)

```python
import os
import cones

# 1. Initialize System and Load Built-ins
sys_inst = cones.System()
sys_inst.constant_registry().load_standard_constants()
sys_inst.substance_manager().register_ideal_gasses()
cones.register_builtin_functions(sys_inst.function_registry(), sys_inst.substance_manager())

# 2. Material Loading
materials_dir = os.path.join(os.path.dirname(__file__), "materials")
sys_inst.substance_manager().load_materials(materials_dir)

# 3. Direct Lexer and Parser Access
script = "T = 300 [K]\nP = T * 2"
lexer = cones.Lexer(script)
tokens = lexer.scan_tokens()
parser = cones.Parser(tokens, sys_inst, os.path.dirname(__file__))
parser.parse()

# 4. Numerical Execution (Solving)
solver = cones.NewtonSolver(tol=1e-9, max_iter=100, verbose=False)
report = solver.solve(sys_inst)

if report.success:
    print(f"Solved successfully in {report.iterations} iterations!")
    
    # 5. Access System Variables
    vars_reg = sys_inst.registry()
    for i in range(vars_reg.size()):
        v = vars_reg.get_variable(i)
        print(f"{v.name} = {v.value} {v.unit_name}")
    
    # 6. Evaluate residuals and Jacobian matrices directly
    f, j = sys_inst.evaluate()
    print("Residual vector f:", f)
    print("Jacobian matrix j:", j)
else:
    print(f"Solver Error: {report.error_msg}")
```

#### Dual Numbers (Automatic Differentiation)

Directly instantiate and perform math with forward-mode automatic differentiation structures:

```python
import cones

# Construct dual numbers: DualNumber(value, derivative)
d1 = cones.DualNumber(2.0, 1.0)
d2 = cones.DualNumber(3.0, 0.0)

res = cones.sin(d1 * d2)
print("sin(d1 * d2) =", res.val)
print("Derivative d(sin(d1*d2))/dx =", res.der)
```

#### Units and Unit Arithmetic

Manage, check, and multiply units programmatically:

```python
import cones

p_unit = cones.Unit.Pascal()
print("Pascal unit:", p_unit.to_string())

c_unit = cones.Unit.Celsius()
print("Celsius offset to Kelvin:", c_unit.offset)

# Perform unit arithmetic (e.g. creating specific entropy/enthalpy units)
new_unit = p_unit * cones.Unit.Meter()
print("Compatible units:", new_unit.compatible(cones.Unit.Newton()))
```

#### Direct Substance Evaluation

Look up property values directly for Ideal Gases and Tabulated Substances:

```python
import cones

sub_mgr = cones.SubstanceManager()
sub_mgr.register_ideal_gasses()

air = sub_mgr.get("Air")
inputs = [
    cones.PropertyArg(cones.PropertyType.TEMPERATURE, cones.DualNumber(300.0, 0.0)),
    cones.PropertyArg(cones.PropertyType.PRESSURE, cones.DualNumber(101325.0, 0.0))
]

# Evaluate density
density_dn = air.evaluate(cones.PropertyType.DENSITY, inputs)
print("Air Density at 300K, 1 atm:", density_dn.val, "kg/m^3")
```
