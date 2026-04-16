# CoNES: Coupled Nonlinear Equation Solver

A high-performance C++ environment for solving large-scale systems of coupled nonlinear equations, utilizing symbolic-numerical hybrid techniques and a custom interpreted scripting language.

To compile the interpreter, copy the dependencies into CoNES/ and run:

```bash
g++ -O3 -std=c++20 -I . src/main.cpp -o cnes.exe
```

### Interpreter Usage

`cnes.exe` acts as a lightweight virtual machine with the following signature:

```
./cnes.exe [input_file.cnes] [-o output_file.txt] [-v] [--version] [--help]
```

## 1. System of Equations Solver (Core Engine)

The core engine transforms high-level mathematical relations into a solvable numerical problem.

### Symbolic Representation
 - **Expression Trees**: Equations are stored as Abstract Syntax Trees (AST). This facilitates symbolic differentiation and optimization before numerical execution.
 - **Variable Registry**: A centralized manager mapping variable names to indices. Internal operations use index-based access to std::vector<double> for $O(1)$ lookup performance.
 - **Automatic Differentiation (AD)**: * Approach: Forward-mode AD using Dual Numbers.Advantage: Provides exact machine-precision derivatives ($\partial F / \partial x$) without the truncation errors of finite differences or the memory explosion of symbolic expansion.
 
### Structural Optimization

 - **Dependency Analysis**: The solver generates an adjacency matrix of the system.
 - **BLT Decomposition**: Uses Tarjan’s Algorithm to identify Strongly Connected Components (SCCs). This decomposes a large system into a sequence of smaller, independent sub-blocks, significantly reducing the complexity of the Jacobian inversion.
 - **Incidence Checking**: Automatic detection of under-determined or over-determined systems via bipartite matching.

### Numerical Execution

 - **Solver Loop**: Newton-Raphson iteration.
 - **Linear Algebra**: Minimalist LU Decomposition with partial pivoting for solving the linear system $J \cdot \Delta x = -F$.
 - **Robustness Features**:
   - **Line Search**: Backtracking line search to ensure the residual decreases at every step, preventing divergence.
   - **Variable Bounding**: Hard-coded physical limits (e.g., $T > 0$) to prevent mathematical domain errors (e.g., $\sqrt{-1}$).

## 2. CNES Script (Interpreter)

A domain-specific language (DSL) designed for clear equation entry and property calls.

### Architectural Pipeline

1. **Lexer/Scanner**: Tokenizes raw text into mathematical symbols, identifiers, and literals.
2. **Recursive Descent Parser**: Converts tokens into the AST. It handles operator precedence (BODMAS/PEMDAS) and validates syntax.
3. **Static Semantic Analysis**: Validates that all variables are defined and that function calls (like steam tables) have the correct number of arguments.
4. **Bytecode Generator**: Flattens the AST into a linear stack-based bytecode to be executed by a lightweight Virtual Machine (VM) during solver iterations.

### Language Features

 - **Implicit Equations**: Supports $f(x) = g(x)$ syntax rather than just assignments.
 - **External Functions**: Native support for thermophysical property calls (e.g., h = enthalpy(Steam, T=400, P=200)).
 - **Comments & Formatting**: Support for documentation within the script to improve readability.

## 3. Data Flow Overview

1. **Input**: User provides a .cnes script.
2. **Parse Phase**: The Script Interpreter builds the AST and registers variables.
3. **Analysis Phase**: The Solver analyzes the AST to form the Jacobian and applies BLT decomposition.
4. **Execution Phase**: The Numerical Engine iterates until the residual vector norm is below the tolerance $\epsilon$.
5. **Output**: Final variable values and convergence statistics are reported.

## 4. Developer Ease

CoNES features a native `.vsix` language extension for VS Code. The packaged extension is available at `src\lang\cnes\cnes-0.0.1.vsix`, and the source at `src\lang\cnes\`. With the extension installed, `.cnes` files can be automatically identified and have their contents parsed and highlighted appropriately. Currently, there is no linting feature. To install locally:

```bash
code --install-extension [path\to\cnes]\cnes-0.0.1.vsix
```

## 5. Development Roadmap

 - **Phase 1**: Define the Expression AST using `std::variant` or inheritance.
 - **Phase 2**: Use Eigen for the linear step ($J \Delta x = -F$).
 - **Phase 3**: Implement the Bipartite Matching (to ensure the user provided enough equations).
 - **Phase 4**: Implement Tarjan's SCC to automatically group the equations into the blocks you desire.
