# CoNES: Coupled Nonlinear Equation Solver

A high-performance C++ environment for solving large-scale systems of coupled nonlinear equations, specializing in thermophysical systems. CoNES utilizes a symbolic-numerical hybrid approach with a custom interpreted scripting language.

To compile the interpreter, ensure Eigen is in the project root and run:

**MacOS / Linux (g++):**
```bash
g++ -O3 -std=c++20 -I . src/main.cpp -o cnes
```

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
 - `--json`: Output results as a JSON object (ideal for GUI/tool integration).
 - `--lint`: Performs lexical and syntactic analysis and variable registration, but stops before solving. 
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
 - **Solver Loop**: Newton-Raphson iteration with backtracking line search.
 - **Robustness Features**:
   - **Variable Bounding**: Hard-coded physical limits (e.g., $T > 0$) prevent mathematical domain errors.
   - **Heuristic Guessing**: Automatically suggests ballpark initial guesses based on assigned units (e.g., 101 kPa for Pressure) to avoid singularities like division by zero.

## 2. Thermophysical Property System

CoNES features a modular, high-performance property engine designed for "EES-like" ease of use.

### Multi-Axis Tabulated Data
 - **Independent Axis Selection**: Properties are gridded on the most stable axes (typically $P$ and $T$).
 - **Inverted Lookups**: Supports direct lookups for $T(P, h)$ and $T(P, s)$ using pre-computed inverted grids, bypassing the need for nested iterations.
 - **Saturation Support**: Automated redirection for saturation lookups. Using `Pressure(Water, T=T1, x=0.5)` automatically utilizes $P_{sat}(T)$ 1D tables.
 - **Two-Phase Properties**: High-speed calculation of two-phase enthalpy, entropy, etc., via $hf + x(hg - hf)$ using saturated liquid/vapor boundary tables.

### Material Support
 - **Ideal Gases**: Analytical models for Air and other simple gases.
 - **Tabulated Substances**: Gridded binary data (`.cnesbin`) for Water, R134a, R12, and more.

## 3. CNES Script (Interpreter)

A domain-specific language (DSL) designed for clear equation entry and property calls.

 - **Implicit Equations**: Supports `f(x) = g(x)` syntax.
 - **Unit System**: Full support for SI and common engineering units (C, bar, kJ/kg, kW). Automatic conversion to internal SI representation.
 - **Inclusion System**: Robust modularity with `include`.
   - **Search Path**: Searches (1) relative to the script, (2) Current Working Directory, and (3) `[exe]/libs/`.
   - **Auto-Extension**: Automatically infers `.cnes` if omitted (e.g., `include "fluid_lib"`).
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
 - **[TODO]** BLT Decomposition (Tarjan's SCC) for block-solving.
 - **[TODO]** Bipartite Matching for DOF validation.
 - **[TODO]** Psychrometrics!

## 5. VS Code Extension

Install the packaged extension at `src/lang/cnes/cnes-0.1.1.vsix`:

```bash
code --install-extension src/lang/cnes/cnes-0.1.1.vsix
```

## 6. Binary Table Creation

CoNES uses properties sourced from [CoolProp](https://coolprop.org), an open source database. To build the required binary tables to use fluids like `Water` or refrigerants like `R134a`, you must have CoolProp installed with `pip`. Run the `export_props` python script from `CoNES/`:

```bash
python3 tools/export_props.py
```

The `materials/` directory should begin to populate with a selection of `.cnesbin` binary substance tables, and the python script's output should confirm the successful creation of each one. Without creating these tables, CoNES will only have access to idea gases.