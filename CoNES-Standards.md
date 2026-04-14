# CoNES (Coupled Nonlinear Equation Solver) Language Standard v1.0

This document defines the formal specification for the `.cnes` scripting language.

## 1. Lexical Grammar

### 1.1 Tokens
- **IDENTIFIER**: `[a-zA-Z_][a-zA-Z0-9_]*` (e.g., `T_out`, `P_1`, `enthalpy`)
- **NUMBER**: `[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?` (e.g., `300`, `273.15`, `1.2e-5`)
- **OPERATORS**: 
    - `:=` (Definition/Fixing)
    - `=` (Equivalence/Equation)
    - `+`, `-`, `*`, `/`, `^` (Arithmetic)
    - `(`, `)` (Grouping)
    - `{`, `}` (Attribute Block)
    - `[`, `]` (Unit Block)
    - `:`, `,`, `.` (Punctuation)
- **SPECIAL**:
    - `_` (Underscore): Stand-in for "Default/Unknown" in attribute blocks.

### 1.2 Comments
- `//` : Single-line comment.
- `/* ... */` : Multi-line block comment.

---

## 2. Syntactic Grammar

### 2.1 Variable Definitions (`:=`)
Used to fix a variable's value or metadata without adding an equation to the Jacobian.
- **Fixed Value**: `x := 300 [K]` (Sets value to 300 and `is_fixed = true`)
- **Metadata Definition**: `x.lower := 0` (Sets metadata, stays floating)
- **Attribute Block**: `x := {300 : 0 : _}` (Sets `{guess : min : max}`)

### 2.2 Equations (`=`)
Used to define mathematical relationships. Adds a residual row to the solver.
- **Syntax**: `Expression = Expression`
- **Example**: `P * V = n * R * T`

### 2.3 Attributes & Dot-Notation
- `.guess` : The starting value for the solver.
- `.lower` : The minimum allowable value (clipping).
- `.upper` : The maximum allowable value (clipping).
- `.unit` : The unit string (e.g., `[K]`).

---

## 3. Semantics & Solver Logic

### 3.1 Degrees of Freedom (DOF)
- A variable is **Fixed** if defined via `:=` (e.g., `x := 5`).
- A variable is **Floating** if it appears in an equation (`=`) or attribute block and has not been fixed.
- **Constraint**: Total Equations (`=`) must equal Total Floating Variables.

### 3.2 Evaluation Order
1. **Metadata Phase**: All `:=` definitions are processed to populate the `VariableRegistry`.
2. **Equation Phase**: All `=` equations are parsed into AST nodes.
3. **Validation Phase**: Check DOF and ensure no circular definitions in metadata.
4. **Execution Phase**: Hand over the `System` object to the `NewtonSolver`.
