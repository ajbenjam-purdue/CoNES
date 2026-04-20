# CoNES (Coupled Nonlinear Equation Solver) Language Standard v1.2

This document defines the formal specification for the `.cnes` scripting language, optimized for high-performance thermophysical system modeling.

## 1. Lexical Grammar

### 1.1 Tokens
- **IDENTIFIER**: `[a-zA-Z_][a-zA-Z0-9_]*` (e.g., `T_out`, `P_1`, `enthalpy`)
- **NUMBER**: `[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?` (e.g., `300`, `273.15`, `1.2e-5`)
- **STRING**: `"..."` (e.g., `"lib/constants.cnes"`) - used for file inclusions.
- **KEYWORDS**: `include`, `routine`, `function`, `return`, `end`
- **OPERATORS**: `:=`, `=`, `+`, `-`, `*`, `/`, `^`, `(`, `)`, `{`, `}`, `[`, `]`, `:`, `,`, `.`

### 1.2 Comments
- `//` : Single-line comment.
- `/* ... */` : Multi-line block comment.

---

## 2. Syntactic Grammar

### 2.1 File Inclusions (`include`)
- **Syntax**: `include "filename"`
- **Extension Inference**: If the extension is omitted, `.cnes` is automatically appended.
- **Search Path Logic**:
  1. **Relative to Parent**: Checks the directory of the file currently being parsed.
  2. **Working Directory**: Checks the current directory of the execution process.
  3. **Standard Library**: Checks the `libs/` folder relative to the `cnes.exe` binary.
- **Behavior**: Pre-parsing directive that recursively parses the target file's definitions and equations into the global context.

### 2.2 Variable Definitions (`:=`)
- **Fixed Value**: `x := 300 [K]` (Fixes value and excludes from solver).
- **Metadata Definition**: `x.guess := 300` (Sets starting value).
- **Attribute Block**: `x := {guess : min : max}`.

### 2.3 Equations (`=`)
- **Syntax**: `Expression = Expression`
- **Unit Inheritance**: If a variable on the LHS has no unit, it inherits the unit of the RHS expression. Inheritance triggers a **suggested guess** based on the unit's physical magnitude.

---

## 3. User-Defined Blocks (Modularity)

### 3.1 Routines
Routines are **macro-style templates**. When called, the parameters are replaced by the provided argument identifiers, and the body tokens are pasted directly into the global solver scope.

- **Definition**:
  ```ees
  routine my_routine(p1, p2)
      Equation1 = p1 * ...
      Equation2 = p2 + ...
  end routine
  ```
- **Call**: `my_routine(VarA, VarB)`

### 3.2 Functions
Functions are **procedural blocks**. They execute sequentially in an isolated local scope. Variables defined inside a function (except the return value) are not visible to the global solver.

- **Definition**:
  ```ees
  function my_procedural_calc(x, y)
      temp = x^2 + y^2
      return temp [J]
  end function
  ```
- **Call**: `Result = my_procedural_calc(10, 20)`
- **Constraint**: Return units are recommended for dimensional consistency.

---

## 4. Units and Casting

### 4.1 SI-Internal Representation
CoNES operates entirely in SI ($kg, m, s, K, Pa, J, W, mol$).

### 4.2 Supported Units
- **Length**: `m`, `km`, `cm`, `mm`
- **Temperature**: `K`, `C`
- **Pressure**: `Pa`, `kPa`, `MPa`, `bar`, `mbar`
- **Energy/Power**: `J`, `kJ`, `W`, `kW`
- **Specific Energy**: `J/kg`, `kJ/kg`
- **Specific Entropy**: `J/kg*K`, `kJ/kg*K`
- **Time**: `s`, `min`, `hr`, `ms`, `us`

---

## 5. Property Lookups

### 5.1 Independent Properties
The thermophysical engine supports flexible input pairs. The priority of resolution is:
1. **Direct Axis**: (P, T)
2. **Inverted Axis**: (P, h) or (P, s) using pre-inverted tables.
3. **Saturation**: (T, x) or (P, x) using $hf + x(hg-hf)$.

### 5.2 Positional Substance Identification
When calling `Pressure(R134a, T=T1, x=1)`, the first positional argument is automatically identified as the target substance if its name matches a registered material.

---

## 6. Suggested Script Usage (Best Practices)

To maintain clarity and prevent solver divergence, scripts should follow this structured sequence:

1. **Imports & Inclusions**:
   ```ees
   include "fluids"
   include "heat_exchanger_lib"
   ```

2. **Function & Routine Definitions**:
   *(Internal logic used multiple times across the script)*

3. **Problem Constants & Boundary Conditions**:
   ```ees
   P_amb := 101325 [Pa]
   T_inlet := 25 [C]
   ```

4. **Solver Setup (Guesses & Bounds)**:
   ```ees
   T_out.guess := 300 [K]
   T_out.lower := 250
   ```

5. **Active Program (Equations)**:
   ```ees
   Q_dot = m_dot * (h_out - h_in)
   h_in = Enthalpy(Water, T=T_inlet, P=P_amb)
   ```

---

## 7. Solver Logic

### 7.1 Automatic Heuristic Guessing
If a variable is assigned a unit but lacks a `.guess`, CoNES provides ballpark starting values:
- **Pressure**: 101,325 Pa
- **Temperature**: 293.15 K
- **Specific Enthalpy**: 250,000 J/kg
- **Density**: 1.2 kg/m³
